#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>

#include "headers/ipc_shm.h"

/**
 * @brief Pošle krátku textovú správu klientovi cez pipe.
 *
 * Funkcia zapíše ukončený C-string do zadaného zápisového konca pipe.
 * Je to jednoduchý wrapper pre `write` so zarovnaním na \0.
 *
 * @param pipe_write_fd Deskriptor na zápisový koniec pipe.
 * @param sprava Ukazovateľ na C-string správu (musí byť \0-ukončený).
 */
void posli_log(int pipe_write_fd, const char* sprava) {
    write(pipe_write_fd, sprava, strlen(sprava) + 1); //+1 kvoli \0
}

/**
 * @brief Uloží výsledky simulácie a konfiguráciu sveta do súboru.
 *
 * Ak `shm->nazov_suboru` je prázdny reťazec, funkcia nič nerobí.
 * Do súboru uloží rozmery mapy, nastavenia replikácií a krokov, počet
 * prekážok, pravdepodobnosti pohybov, mapu sveta (0 = prázdne, 1 = prekážka)
 * a agregované výsledky pre každé políčko (priemerný počet krokov a
 * percento úspešnosti).
 *
 * @param shm Ukazovateľ na zdieľanú štruktúru obsahujúcu výsledky a konfiguráciu.
 */
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

/**
 * @brief Načíta konfiguráciu a mapu sveta zo súboru do zdieľanej pamäte.
 *
 * Očakáva formát uložený funkciou `uloz_vysledky_do_suboru`. V prípade
 * chyby pri čítaní alebo parsovaní vráti false a zatvorí súbor.
 *
 * @param shm Ukazovateľ na zdieľanú štruktúru, kam sa načítajú hodnoty.
 * @return true ak bolo načítanie úspešné, inak false.
 */
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

/**
 * @brief Overí, či je svet priechodný (všetky neprekážkové políčka dosiahnuteľné z [0,0]).
 *
 * Používa BFS na toroidnej mriežke (okraje sa "zabalia"). Porovná počet
 * navštívených políčok s počtom voľných políčok v mape.
 *
 * @param shm Ukazovateľ na zdieľanú štruktúru obsahujúcu mapu a rozmery.
 * @return true ak sú všetky neprekážkové políčka dosiahnuteľné, inak false.
 */
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

/**
 * @brief Vyberie smer pohybu na základe nastavených pravdepodobností.
 *
 * Generuje náhodné číslo v [0,1) a vráti index smeru podľa kumulatívnych
 * pravdepodobností v poli `shm->pravdepodobnost`.
 * Indexy sú mapované takto: 0 = HORE, 1 = DOLE, 2 = VLAVO, 3 = VPRAVO.
 * Ako poistka vráti 3 (VPRAVO) pre prípad zaokrúhľovacích chýb.
 *
 * @param shm Ukazovateľ na zdieľanú štruktúru obsahujúcu pole pravdepodobností.
 * @return Číslo v rozsahu 0..3 reprezentujúce smer.
 */
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

/**
 * @brief Vygeneruje náhodný svet s prekážkami podľa percenta.
 *
 * Každé políčko (okrem cieľa [0,0] a stredu mapy) sa nastaví na prekážku
 * s pravdepodobnosťou `percento_prekazok` percent. Hodnoty sa ukladajú do
 * `shm->svet` (PRAZDNE alebo PREKAZKA).
 *
 * @param shm Ukazovateľ na zdieľanú štruktúru obsahujúcu rozmery a mapu.
 * @param percento_prekazok Celé percento (0-100) šance, že políčko bude prekážka.
 */
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

/**
 * @brief Simuluje jedného chodca z daného štartovacieho políčka.
 *
 * Hlavný simulačný cyklus pohybuje chodcom, kontroluje kolízie s prekážkami
 * a v interaktívnom režime aktualizuje pozíciu cez semafory pre klienta.
 * Po skončení replikácie aktualizuje štatistiky (priemerné kroky a počet úspechov)
 * pre štartovacie políčko.
 *
 * @param shm Ukazovateľ na zdieľanú štruktúru s konfiguráciou a výsledkami.
 * @param start_r Počiatočný riadok chodca.
 * @param start_s Počiatočný stĺpec chodca.
 */
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

/**
 * @brief Inicializuje herný svet pre server.
 * * Načíta svet zo súboru pri opätovnom spustení, alebo vygeneruje nový náhodný svet
 * s prekážkami a overí jeho priechodnosť pomocou BFS.
 * * @param shm Smerník na zdieľanú pamäť.
 * @return true ak bol svet úspešne inicializovaný, false pri chybe alebo požiadavke na stop.
 */
bool inicializuj_svet_servera(ZdielaneData_t* shm) {
    if (shm->opetovne_spustenie) {
        if (!nacitaj_konfig_zo_suboru(shm)) {
            printf("[SERVER] Chyba: nepodarilo sa nacitat subor %s\n", shm->nazov_suboru);
            return false;
        }
        printf("[SERVER] Svet uspesne nacitany zo suboru\n");
    } else {
        int pokusy_generovania = 0;
        do {
            generuj_svet_s_prekazkami(shm, shm->pocet_prekazok);
            pokusy_generovania++;

            if (shm->stav == SIM_STOP_REQUESTED) return false;
        } while (!je_svet_validny(shm));

        printf("[SERVER] Svet vygenerovany na %d. pokus.\n", pokusy_generovania);
    }
    return true;
}

/**
 * @brief Vykoná kompletnú sumárnu simuláciu pre všetky políčka sveta.
 * * Pre každú replikáciu prejde všetky políčka mriežky. Ak políčko nie je prekážka,
 * spustí z neho simuláciu náhodnej chôdze. Špeciálne ošetruje cieľový bod [0,0].
 * * @param shm Smerník na zdieľanú pamäť.
 */
void vykonaj_sumarnu_simulaciu(ZdielaneData_t* shm, int pipe_write_fd) {
    // Reset výsledkov v zdieľanej pamäti pod mutexom
    sem_wait(&shm->shm_mutex);
    for(int r = 0; r < shm->riadky; r++) {
        for(int s = 0; s < shm->stlpece; s++) {
            shm->vysledky[r][s].avg_kroky = 0;
            shm->vysledky[r][s].pravdepodobnost_dosiahnutia = 0;
        }
    }
    sem_post(&shm->shm_mutex);

    // Hlavný cyklus replikácií
    for (int r_id = 0; r_id < shm->total_replikacie; r_id++) {
        printf("\033[H\033[J");
        if (shm->stav == SIM_STOP_REQUESTED) {
            write(pipe_write_fd, "SERVER: Zastavujem vypocty na ziadost klienta.", 46);
            return;
        }
        shm->aktualne_replikacie = r_id;

        // V server_logic.c v hlavnom cykle replikácií
        if (shm->aktualne_replikacie % 1000 == 0) {
            usleep(1); // Na mikrosekundu uvoľní procesor pre klienta
        }

        for (int riadok = 0; riadok < shm->riadky; riadok++) {
            for (int stlpec = 0; stlpec < shm->stlpece; stlpec++) {
                // Kontrola v každom políčku (veľmi dôležité pre veľké mapy)
                if (shm->stav == SIM_STOP_REQUESTED) {
                    // Log už posielam o úroveň vyššie, tu stačí return
                    return;
                }

                // Bod [0,0] je cieľ - automaticky 100% úspešnosť, 0 krokov
                if (riadok == 0 && stlpec == 0) {
                    if (r_id == 0) {
                        sem_wait(&shm->shm_mutex);
                        shm->vysledky[riadok][stlpec].pravdepodobnost_dosiahnutia = shm->total_replikacie;
                        shm->vysledky[riadok][stlpec].avg_kroky = 0;
                        sem_post(&shm->shm_mutex);
                    }
                    continue; // Simulácia chôdze pre cieľ sa nespúšťa
                }

                if (shm->svet[riadok][stlpec] != PREKAZKA) {
                    simuluj_chodzu_z_policka(shm, riadok, stlpec);
                }
            }
        }

        // Po dokončení jednej replikácie: pošli notifikáciu klientovi a krátky log cez pipe
        // Aktualizujeme ukazovateľ aktualne_replikacie pod mutexom pre konzistenciu.
        sem_wait(&shm->shm_mutex);
        shm->aktualne_replikacie = r_id; // index tej prave dokončenej replikacie (0-based)
        sem_post(&shm->shm_mutex);

        // Pošli krátku správu do pipe, aby sa klient mohol informovať aj textovo.
        if (pipe_write_fd >= 0) {
            char msg[128];
            snprintf(msg, sizeof(msg), "SERVER: Dokoncena replikacia %d/%d", r_id + 1, shm->total_replikacie);
            posli_log(pipe_write_fd, msg);
        }

        // Prebudíme klienta, aby vykreslil aktuálny stav (vrátane aktualne_replikacie)
        sem_post(&shm->data_ready);
        // Krátke pozastavenie, aby mal používateľ čas prečítať notifikáciu / update
        usleep(10000);
    }
}

/**
 * @brief Hlavná riadiaca logika servera.
 * * Zabezpečuje čakanie na klienta, inicializáciu simulácie, spustenie zvoleného
 * módu (interaktívny/sumárny) a finálne uloženie výsledkov.
 * * @param shm Smerník na zdieľanú pamäť.
 */
//TODO mozno to treba upravit tak aby som si vybral umiestnenie chodza a ukazal cestu do ciela
void spusti_server(ZdielaneData_t* shm, int pipe_write_fd) {
    printf("[SERVER] Čakám na inicializáciu menu klientom...\n");

    // Aktívne čakanie na štart z menu
    while (shm->stav != SIM_INIT) {
        if (shm->stav == SIM_STOP_REQUESTED) return;
        usleep(10000);
    }
    srand(time(NULL));

    posli_log(pipe_write_fd, "SERVER: inicializujem svet...");

    // Príprava sveta (načítanie/generovanie)
    if (!inicializuj_svet_servera(shm)) {
        if (shm->stav != SIM_STOP_REQUESTED) {
            posli_log(pipe_write_fd, "Server: Chyba pri inicializacii sveta!");
        } else {
            posli_log(pipe_write_fd, "Server: Inicializacia zrusena pouzivatelom.");
        }

        sem_wait(&shm->shm_mutex); // Zabezpečenie konzistencie stavu
        shm->stav = SIM_FINISHED;
        sem_post(&shm->shm_mutex);

        sem_post(&shm->data_ready);
        return;
    }

    posli_log(pipe_write_fd, "Server: Svet pripraveny, startujem simulaciu.");
    shm->stav = SIM_RUNNING;

    if (shm->mod == INTERAKTIVNY) {
        // Spustenie jednej trajektórie zo stredu mapy
        posli_log(pipe_write_fd, "Server: Bezi interaktivny mod...");
        int start_r = shm->riadky / 2;
        int start_s = shm->stlpece / 2;
        shm->aktualne_replikacie = 0;

        simuluj_chodzu_z_policka(shm, start_r, start_s);
        usleep(30000); // Krátka pauza na doznenie vizualizácie
    } else {
        // Hromadný výpočet pre všetky políčka
        posli_log(pipe_write_fd, "Server: Bezi vypocet sumarneho modu...");
        vykonaj_sumarnu_simulaciu(shm, pipe_write_fd);
    }

    // Finálne uloženie dát a upratovanie stavu
    if (shm->stav != SIM_STOP_REQUESTED) {
        posli_log(pipe_write_fd, "Server: Ukladam vysledky do suboru...");
        uloz_vysledky_do_suboru(shm);
        // Simulácia dobehla prirodzene -> nastavíme FINISHED
        sem_wait(&shm->shm_mutex);
        shm->stav = SIM_FINISHED;
        sem_post(&shm->shm_mutex);
        posli_log(pipe_write_fd, "Server: Simulacia uspesne ukoncena.");
    } else {
        posli_log(pipe_write_fd, "Server: Simulacia prerusena pouzivatelom.");
        // Stav zostáva SIM_STOP_REQUESTED, klient sa vráti do menu
    }

    // Prebudenie klienta pre finálne zobrazenie alebo pre návrat do menu
    sem_post(&shm->data_ready);
    printf("[SERVER] Simulácia ukončená.\n");
}