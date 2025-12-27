#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include "headers/ipc_shm.h"

void uloz_vysledky_do_suboru(ZdielaneData_t* shm) {
    //ak nezadal nazov
    if (shm->nazov_suboru[0] == '\0') {
        return;
    }

    FILE * file = fopen(shm->nazov_suboru, "w");
    if (file == NULL) {
        perror("Nepodarilo sa otvorit subor na zapis");
        return;
    }

    //uloz zakladne parametre
    fprintf(file, "%d %d\n", shm->riadky, shm->stlpece);
    fprintf(file, "%d %d\n", shm->total_replikacie, shm->K_max_kroky);
    fprintf(file, "%d\n", shm->pocet_prekazok); // preistotu aj ked nacitavam tu ulozenu mapu
    fprintf(file, "%f %f %f %f \n", shm->pravdepodobnost[0], shm->pravdepodobnost[1], shm->pravdepodobnost[2], shm->pravdepodobnost[3]);

    //ulozenie mapy sveta (0 = prazdne, 1 = prekazka)
    for (int i = 0; i < shm->riadky; i++) {
        for (int j = 0; j < shm->stlpece; j++) {
            fprintf(file, "%d ", shm->svet[i][j]);
        }
        fprintf(file, "\n");
    }

    //ulozenie vysledkov
    fprintf(file, "--- VYSLEDKY ---\n");
    for (int i = 0; i < shm->riadky; i++) {
        for (int j = 0; j < shm->stlpece; j++) {
            double avg = (double)shm->vysledky[i][j].avg_kroky / shm->total_replikacie;
            double pravdepodobnost = ((double)shm->vysledky[i][j].pravdepodobnost_dosiahnutia / shm->total_replikacie * 100);
            fprintf(file, "%.2f(%.0f%%) ", avg, pravdepodobnost);
        }
        fprintf(file, "\n");
    }
    fclose(file);
    printf("[SERVER] Vysledky boli ulozene do suboru: %s\n", shm->nazov_suboru);
}

bool nacitaj_konfig_zo_suboru(ZdielaneData_t* shm) {
    FILE* file = fopen(shm->nazov_suboru, "r");
    if (file == NULL) {
        perror("Nepodarilo sa otvorit subor na citanie");
        return false;
    }

    //nacitanie zakladnych parametrov (riadky, stlpce, replikacie, kroky)
    if (fscanf(file, "%d %d", &shm->riadky, &shm->stlpece) != 2) {
        fclose(file);
        return false;
    }
    if (fscanf(file, "%d %d", &shm->total_replikacie, &shm->K_max_kroky) != 2) {
        fclose(file);
        return false;
    }

    //nacitanie hustoty prekazok aj ked ju pri nacitani nepouzivam
    if (fscanf(file, "%d", &shm->pocet_prekazok) != 1) {
        fclose(file);
        return false;
    }

    //nacitanie pravdepodobnosti pohybu (pouzivam float docasne kvoli fscanf potom convert do double)
    float p0, p1, p2, p3;
    if (fscanf(file, "%f %f %f %f", &p0, &p1, &p2, &p3) != 4) {
        fclose(file);
        return false;
    }
    shm->pravdepodobnost[0] = p0;
    shm->pravdepodobnost[1] = p1;
    shm->pravdepodobnost[2] = p2;
    shm->pravdepodobnost[3] = p3;

    //nacitanie mapy sveta (0 = prazdne, 1 = prekazka)
    for (int i = 0; i < shm->riadky; i++) {
        for (int j = 0; j < shm->stlpece; j++) {
            int hodnota;
            if (fscanf(file, "%d", &hodnota) != 1) {
                fclose(file);
                return false;
            }
            shm->svet[i][j] = hodnota;
        }
    }
    //vysledky ma nezaujimaju pre novu simulacii takze zatvaram
    fclose(file);
    return true;
}

bool je_svet_validny(ZdielaneData_t* shm) {
    int riadky = shm->riadky;
    int stlpce = shm->stlpece;

    //spocitanie kolko volnych policok vratane ciela v mape je
    int celkovy_pocet = 0;
    for (int riadok = 0; riadok < riadky; riadok++) {
        for (int stlpec = 0; stlpec < stlpce; stlpec++) {
            if (shm->svet[riadok][stlpec] != PREKAZKA) {
                celkovy_pocet++;
            }
        }
    }

    //BFS priprava
    bool navstivene[MAX_ROWS][MAX_COLS] = {false};
    int front_r[MAX_ROWS * MAX_COLS];
    int front_s[MAX_ROWS * MAX_COLS];
    int zaciatok = 0;
    int koniec = 0;

    //start z [0,0]
    front_r[koniec] = 0;
    front_s[koniec] = 0;
    koniec++;
    navstivene[0][0] = true;
    int dosiahnutelnych = 1;

    //rozlievanie cez BFS
    while (zaciatok < koniec) {
        int riadok = front_r[zaciatok];
        int stlpec = front_s[zaciatok];  //oprava indexu po zaciatok++

        zaciatok++;

        int posun_riadok[] = {-1, 1, 0, 0};
        int posun_stlpec[] = {0, 0, -1, 1};

        for (int i = 0; i < 4; i++) {
            //toroidny sused
            int novy_riadok = ((riadok + posun_riadok[i] + riadky) % riadky);
            int novy_stlpec = ((stlpec + posun_stlpec[i] + stlpce) % stlpce);

            if (shm->svet[novy_riadok][novy_stlpec] != PREKAZKA && !navstivene[novy_riadok][novy_stlpec]) {
                navstivene[novy_riadok][novy_stlpec] = true;
                front_r[koniec] = novy_riadok;
                front_s[koniec] = novy_stlpec;
                koniec++;
                dosiahnutelnych++;
            }
        }
    }
    //ak som sa dostal na vsetky volne policka mapa je v poriadku
    return (dosiahnutelnych == celkovy_pocet);
}

int vyber_smeru(ZdielaneData_t* shm) {
    double r = (double)rand() / RAND_MAX;
    double kumulativna_suma = 0;

    for (int i = 0; i < 4; i++) {
        kumulativna_suma += shm->pravdepodobnost[i];
        if (r <= kumulativna_suma) {
            return i; //vrati 0(HORE) 1(DOLE) 2(Vlavo) 3(Vpravo)
        }
    }
    return 3; // Poistka pre zaokrúhľovacie chyby (Vpravo)
}

void generuj_svet_s_prekazkami(ZdielaneData_t* shm, int percento_prekazok) {
    for (int riadok = 0; riadok < shm->riadky; riadok++) {
        for (int stlpec = 0; stlpec < shm->stlpece; stlpec++) {

            //ochrana aby ciel [0,0] nemohol byt prekazka
            if (riadok == 0 && stlpec == 0) {
                shm->svet[riadok][stlpec] = PRAZDNE;
                continue;
            }

            //vynechanie stred mriezky aby chodec nezacinal v stene
            if (riadok == shm->riadky / 2 && stlpec == shm->stlpece / 2) {
                shm->svet[riadok][stlpec] = PRAZDNE;
                continue;
            }
            //nahodne rozhodnutie ci na policku bude prekazka
            if ((rand() % 100) < percento_prekazok) {
                shm->svet[riadok][stlpec] = PREKAZKA;
            } else {
                shm->svet[riadok][stlpec] = PRAZDNE;
            }
        }
    }
}

//TODO vykresli vzdy o jednu mapu navyse cize ked dam krokov 5 tak najskor sa vykresli zaciatok a potom 5 krokov, a na konci sa vykresli finalna mapa cize to je este o jednu navyse
void simuluj_chodzu_z_policka(ZdielaneData_t* shm, int start_r, int start_s) {
    int aktualny_r = start_r;
    int aktualny_s = start_s;
    int pocet_krok = 0;

    //zobrazenie startovacej pozicie chodcu
    if (shm->mod == INTERAKTIVNY) {
        sem_wait(&shm->shm_mutex);
        shm->aktualna_pozicia_chodca.riadok = aktualny_r;
        shm->aktualna_pozicia_chodca.stlpec = aktualny_s;
        sem_post(&shm->shm_mutex);

        sem_post(&shm->data_ready);
        usleep(200000);
    }

    //chodec ide kym nieje v cieli alebo neprekroci pocet K
    while ((aktualny_r != 0 || aktualny_s != 0) && pocet_krok < shm->K_max_kroky) {
        if (shm->stav == SIM_STOP_REQUESTED) {
            return;
        }
        int smer = vyber_smeru(shm) % 4;
        int buduci_r = aktualny_r;
        int buduci_s = aktualny_s;

        //vypocet buducej pozicie s toroidnym efektom (BOD 3)
        switch (smer) {
            case 0: // HORE
                buduci_r = (aktualny_r - 1 + shm->riadky) % shm->riadky;
                break;
            case 1: // DOLE
                buduci_r = (aktualny_r + 1) % shm->riadky;
                break;
            case 2: // VLAVO
                buduci_s = (aktualny_s - 1 + shm->stlpece) % shm->stlpece;
                break;
            case 3: // VPRAVO
                buduci_s = (aktualny_s + 1) % shm->stlpece;
                break;
            default:
                return;
        }

        // Kontrola prekážky: Ak na cieľovom políčku nie je stena, pohni sa
        if (shm->svet[buduci_r][buduci_s] != PREKAZKA) {
            aktualny_r = buduci_r;
            aktualny_s = buduci_s;

            sem_wait(&shm->shm_mutex);
            shm->aktualna_pozicia_chodca.riadok = aktualny_r;
            shm->aktualna_pozicia_chodca.stlpec = aktualny_s;
            sem_post(&shm->shm_mutex);
        }

        pocet_krok++;

        //ak je INTERAKTIVNY mod musi signalizovat klientovi
        if (shm->mod == INTERAKTIVNY) {
            sem_post(&shm->data_ready);
            usleep(100000);
        }
    }
    // STATISTIKY
    sem_wait(&shm->shm_mutex);

    // PRIČÍTAŠ K ŠTARTOVACÍM SÚRADNICIAM, nie k aktuálnym (ktoré sú 0,0)
    shm->vysledky[start_r][start_s].avg_kroky += pocet_krok;

    // Ak dosiel do ciela [0,0], zvysi pocitadlo uspechov
    if (aktualny_r == 0 && aktualny_s == 0) {
        shm->vysledky[start_r][start_s].pravdepodobnost_dosiahnutia++;
    }
    sem_post(&shm->shm_mutex);
}

void spusti_server(ZdielaneData_t* shm) {
    printf("[SERVER] Čakám na inicializáciu menu klientom...\n");
    // SERVER TU BUDE ČAKAŤ, kým klient nenastaví SIM_INIT
    while (shm->stav != SIM_INIT) {
        if (shm->stav == SIM_STOP_REQUESTED) return;
        usleep(100000); // 100ms pauza, aby sme nevyťažili procesor
    }
    srand(time(NULL));

    if (shm->opetovne_spustenie) {

        if (!nacitaj_konfig_zo_suboru(shm)) {
            printf("[SERVER] Chyba: nepodarilo sa nacitat subor %s\n", shm->nazov_suboru);
            shm->stav = SIM_FINISHED;
            sem_post(&shm->data_ready);
            return;
        }
        printf("[SERVER] Svet uspesne nacitany zo suboru\n");
    } else {
        int pokusy_generovania = 0;
        do {
            generuj_svet_s_prekazkami(shm, shm->pocet_prekazok);
            pokusy_generovania++;
            // Kontrola, či užívateľ neukončil program počas generovania (ak by trvalo dlho)
            if (shm->stav == SIM_STOP_REQUESTED) return;
        } while (!je_svet_validny(shm));
        printf("[SERVER] Svet vygenerovany na %d. pokus.\n", pokusy_generovania);
    }

    //nastavenia PC stavu
    printf("[SERVER] Svet uspesne overeny cez BFS. Startujem simulaciu...\n");
    shm->stav = SIM_RUNNING;

    if (shm->mod == INTERAKTIVNY) {
        //TODO mozno to treba upravit tak aby som si vybral umiestnenie chodza a ukazal cestu do ciela
        int start_r = shm->riadky / 2;
        int start_s = shm->stlpece / 2;

        shm->aktualna_pozicia_chodca.riadok = start_r;
        shm->aktualna_pozicia_chodca.stlpec = start_s;
        shm->aktualne_replikacie = 0;

        simuluj_chodzu_z_policka(shm, start_r, start_s);

        usleep(300000);
    } else {
        //vycistenie vysledkov pre zaciatkom
        for(int r = 0; r < shm->riadky; r++) {
            for(int s = 0; s < shm->stlpece; s++) {
                shm->vysledky[r][s].avg_kroky = 0;
                shm->vysledky[r][s].pravdepodobnost_dosiahnutia = 0;
            }
        }

        for (int r_id = 0; r_id < shm->total_replikacie; r_id++) {
            if (shm->stav == SIM_STOP_REQUESTED) {
                break;
            }
            shm->aktualne_replikacie = r_id;
            //toto by aktualizovalo tabulku nonstop (zakomentovane aby to vypocitalo a az potom vykreslilo)
            // sem_wait(&shm->shm_mutex);
            // sem_post(&shm->shm_mutex);

            //cyklus pre sumarny rezim
            //pre kazde policko sveta
            for (int riadok = 0; riadok < shm->riadky; riadok++) {
                for (int stlpec = 0; stlpec < shm->stlpece; stlpec++) {
                    // Kontrola ukončenia vo vnútri cyklov mapy (veľmi dôležité!)
                    if (shm->stav == SIM_STOP_REQUESTED) {
                        goto koniec_simulacie;
                    }
                    if (riadok == 0 && stlpec == 0) {
                        if (r_id == 0) {
                            //pre ciel zapisem 100% uspesnost
                            //pri 1000 replikaciach je to 1000 uspechov
                            shm->vysledky[riadok][stlpec].pravdepodobnost_dosiahnutia = shm->total_replikacie;
                            shm->vysledky[riadok][stlpec].avg_kroky = 0;
                            continue;
                        }
                    }
                    if (shm->svet[riadok][stlpec] == PREKAZKA) {
                        continue;
                    }
                    //spustim jednu cestu z chodca z tohto bodu
                    simuluj_chodzu_z_policka(shm, riadok, stlpec);
                }
            }
            // //po dokonceni vsetkych policok v jednej replikacii
            // sem_post(&shm->data_ready);1
            // usleep(1000000);
        }
    }
    koniec_simulacie:
    if (shm->stav != SIM_STOP_REQUESTED) {
        printf("[SERVER] Ukladám výsledky do súboru...\n");
        uloz_vysledky_do_suboru(shm);
    }
    shm->stav = SIM_FINISHED;
    sem_post(&shm->data_ready); // Posledný signál pre klienta
    printf("[SERVER] Všetky replikácie dokončené.\n");
}