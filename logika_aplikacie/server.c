#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <sys/socket.h>

#include "../headers/ipc_shm.h"

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
    // SYSTÉMOVÉ VOLANIE WRITE:
    // 1. pipe_write_fd: Cieľ, kam zapisujem (v mojom prípade rúra vedúca ku klientovi).
    // 2. sprava: Smerník na začiatok dát v pamäti.
    // 3. strlen(sprava) + 1: Počet bajtov, ktoré sa majú preniesť.

    // DÔLEŽITÉ: '+ 1' pridávam preto, aby som preniesol aj ukončovací znak '\0' (null-terminator).
    // Ak by som ho nepreniesol, klient by nevedel, kde reťazec končí a pri pokuse o výpis
    // by mohol vypísať náhodné "smeti" z pamäte.
    write(pipe_write_fd, sprava, strlen(sprava) + 1);
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
    //kontrola nazvu suboru, ak nezadal funkcia skonci
    if (shm->nazov_suboru[0] == '\0') {
        return;
    }

    // 2. OTVORENIE SÚBORU
    // Režim "w" (write) vytvorí nový súbor alebo prepíše ten pôvodný.
    FILE * file = fopen(shm->nazov_suboru, "w");
    if (file == NULL) {
        perror("Nepodarilo sa otvorit subor na zapis");
        return;
    }

    // 3. ULOŽENIE ZÁKLADNÝCH PARAMETROV
    fprintf(file, "%d %d\n", shm->riadky, shm->stlpece);
    fprintf(file, "%d %d\n", shm->total_replikacie, shm->K_max_kroky);
    fprintf(file, "%d\n", shm->pocet_prekazok); // preistotu aj ked nacitavam tu ulozenu mapu
    fprintf(file, "%f %f %f %f \n", shm->pravdepodobnost[0], shm->pravdepodobnost[1], shm->pravdepodobnost[2], shm->pravdepodobnost[3]);

    // 4. ULOŽENIE MAPY SVETA (0 = prazdne, 1 = prekazka)
    for (int i = 0; i < shm->riadky; i++) {
        for (int j = 0; j < shm->stlpece; j++) {
            fprintf(file, "%d ", shm->svet[i][j]);
        }
        fprintf(file, "\n");
    }

    // 5. VÝPOČET A ULOŽENIE FINÁLNYCH VÝSLEDKOV
    fprintf(file, "--- VYSLEDKY ---\n");
    for (int i = 0; i < shm->riadky; i++) {
        for (int j = 0; j < shm->stlpece; j++) {
            double avg = (double)shm->vysledky[i][j].avg_kroky / shm->total_replikacie;
            double pravdepodobnost = ((double)shm->vysledky[i][j].pravdepodobnost_dosiahnutia / shm->total_replikacie * 100);
            fprintf(file, "%.2f(%.0f%%) ", avg, pravdepodobnost);
        }
        fprintf(file, "\n");
    }
    // 6. ZATVORENIE SÚBORU A POTVRDENIE
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
    // 1. OTVORENIE SÚBORU
    // Používame režim "r" (read - čítanie).
    FILE* file = fopen(shm->nazov_suboru, "r");
    if (file == NULL) {
        perror("Nepodarilo sa otvorit subor na citanie");
        return false;
    }

    // 2. NAČÍTANIE ZÁKLADNÝCH PARAMETROV
    // fscanf vracia počet úspešne načítaných hodnôt.
    // Ak sa nerovná počtu očakávaných premenných (napr. 2), súbor je chybný.
    //nacitanie zakladnych parametrov (riadky, stlpce, replikacie, kroky)
    if (fscanf(file, "%d %d", &shm->riadky, &shm->stlpece) != 2) {
        fclose(file);
        return false;
    }
    if (fscanf(file, "%d %d", &shm->total_replikacie, &shm->K_max_kroky) != 2) {
        fclose(file);
        return false;
    }

    // Načítanie hustoty prekážok (v kontexte načítania zo súboru je to len informatívny údaj)
    if (fscanf(file, "%d", &shm->pocet_prekazok) != 1) {
        fclose(file);
        return false;
    }

    // 3. NAČÍTANIE PRAVDEPODOBNOSTÍ POHYBU
    // Používame dočasné float premenné, pretože fscanf s %f niekedy lepšie spolupracuje
    // s desatinnými číslami v textovom súbore, následne ich priradíme do double poľa v SHM.
    float p0, p1, p2, p3;
    if (fscanf(file, "%f %f %f %f", &p0, &p1, &p2, &p3) != 4) {
        fclose(file);
        return false;
    }
    shm->pravdepodobnost[0] = p0;
    shm->pravdepodobnost[1] = p1;
    shm->pravdepodobnost[2] = p2;
    shm->pravdepodobnost[3] = p3;

    // 4. REKONŠTRUKCIA MAPY SVETA
    // Postupne prechádzame súbor a plníme dvojrozmerné pole shm->svet.
    // Očakávame maticu celých čísel (0 pre voľné políčko, 1 pre prekážku).
    for (int i = 0; i < shm->riadky; i++) {
        for (int j = 0; j < shm->stlpece; j++) {
            int hodnota;
            if (fscanf(file, "%d", &hodnota) != 1) {
                // Ak súbor skončí skôr, než naplníme celú maticu (napr. chýba riadok)
                fclose(file);
                return false;
            }
            shm->svet[i][j] = hodnota;
        }
    }
    // 5. UKONČENIE ČÍTANIA
    // Výsledky uložené v súbore (pod čiarou --- VYSLEDKY ---) pri načítaní ma nezaujímajú,
    // pretože chcem spustiť novú simuláciu s touto mapou.
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

    // 1. SPOČÍTANIE VOĽNÝCH POLÍČOK
    // Najprv zistím, koľko políčok v mape nie je prekážka (teda koľko by som mal navštíviť).
    int celkovy_pocet = 0;
    for (int riadok = 0; riadok < riadky; riadok++) {
        for (int stlpec = 0; stlpec < stlpce; stlpec++) {
            if (shm->svet[riadok][stlpec] != PREKAZKA) {
                celkovy_pocet++;
            }
        }
    }

    // 2. PRÍPRAVA BFS (Fronta a pole navštívených)
    // 'navstivene' bráni tomu, aby som sa točili v kruhu.
    bool navstivene[MAX_ROWS][MAX_COLS] = {false};
    int front_r[MAX_ROWS * MAX_COLS];
    int front_s[MAX_ROWS * MAX_COLS];
    int zaciatok = 0;
    int koniec = 0;

    // Frontu (queue) simulujem dvoma poliami pre riadky a stĺpce.
    front_r[koniec] = 0;
    front_s[koniec] = 0;
    koniec++;
    navstivene[0][0] = true;
    int dosiahnutelnych = 1;

    // 4. ALGORITMUS ROZLIEVANIA (BFS Slučka)
    while (zaciatok < koniec) {
        // Vyberiem aktuálne políčko z fronty
        int riadok = front_r[zaciatok];
        int stlpec = front_s[zaciatok];  //oprava indexu po zaciatok++

        zaciatok++;

        // Smery pohybu: Hore, Dole, Vľavo, Vpravo
        int posun_riadok[] = {-1, 1, 0, 0};
        int posun_stlpec[] = {0, 0, -1, 1};

        for (int i = 0; i < 4; i++) {
            // TOROIDNÝ VÝPOČET SÚSEDNÝCH SÚRADNÍC:
            // Pridaním '+ riadky' a operátorom modulo '%' zabezpečím "pretečenie" cez okraje.
            // Napr. na riadku 0 smerom hore (-1) dostaneme (0 - 1 + 10) % 10 = 9.
            int novy_riadok = ((riadok + posun_riadok[i] + riadky) % riadky);
            int novy_stlpec = ((stlpec + posun_stlpec[i] + stlpce) % stlpce);

            // Ak sused nie je prekážka a ešte som tam nebol, pridám ho do fronty.
            if (shm->svet[novy_riadok][novy_stlpec] != PREKAZKA && !navstivene[novy_riadok][novy_stlpec]) {
                navstivene[novy_riadok][novy_stlpec] = true;
                front_r[koniec] = novy_riadok;
                front_s[koniec] = novy_stlpec;
                koniec++;
                dosiahnutelnych++; // Započítam ďalšie nájdené políčko
            }
        }
    }
    // 5. FINÁLNE OVERENIE
    // Ak sa počet políčok nájdených cez BFS rovná celkovému počtu voľných políčok,
    // znamená to, že svet je plne priechodný a neexistujú v ňom "izolované ostrovy".
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
    // 1. GENEROVANIE NÁHODNÉHO ČÍSLA
    // rand() vráti číslo od 0 do RAND_MAX. Podelením RAND_MAX dostanem
    // hodnotu v intervale [0.0, 1.0].
    double r = (double)rand() / RAND_MAX;
    double kumulativna_suma = 0;

    // 2. RULETOVÝ VÝBER
    // Prechádzam všetky 4 možné smery.
    for (int i = 0; i < 4; i++) {
        // Postupne pripočítavam pravdepodobnosti k sume.
        // Ak mám napr. pravdepodobnosti [0.1, 0.4, 0.3, 0.2], intervaly budú:
        // Hore: [0.0 - 0.1], Dole: [0.1 - 0.5], Vľavo: [0.5 - 0.8], Vpravo: [0.8 - 1.0]
        kumulativna_suma += shm->pravdepodobnost[i];

        // Ak vygenerované číslo 'r' padne do aktuálneho intervalu, vrátime index smeru.
        if (r <= kumulativna_suma) {
            return i; //vrati 0(HORE) 1(DOLE) 2(Vlavo) 3(Vpravo)
        }
    }
    // 3. BEZPEČNOSTNÁ POISTKA
    // Kvôli nepresnosti pri práci s 'double' (floating point errors) sa môže stať,
    // že kumulativna_suma bude napr. 0.99999999999 a 'r' bude 1.0.
    // V takom prípade vrátim posledný možný smer.
    return 3;
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

            // 1. OCHRANA CIEĽA
            // Bod [0,0] je v tejto simulácii cieľom. Ak by tu bola prekážka,
            // chodec by nikdy nemohol úspešne dokončiť svoju cestu.
            if (riadok == 0 && stlpec == 0) {
                shm->svet[riadok][stlpec] = PRAZDNE;
                continue;
            }

            // 2. OCHRANA ŠTARTU
            // Chodec začína v strede mriežky. Ak by sa "narodil" v stene,
            // simulácia by skončila chybou alebo okamžitým uväznením.
            if (riadok == shm->riadky / 2 && stlpec == shm->stlpece / 2) {
                shm->svet[riadok][stlpec] = PRAZDNE;
                continue;
            }

            // 3. NÁHODNÉ GENEROVANIE PREKÁŽKY
            // rand() % 100 vráti číslo od 0 do 99.
            // Ak je toto číslo menšie ako percento_prekazok, políčko sa stane stenou.
            // Príklad: Ak zadám 20%, tak cca každé piate políčko bude prekážka.
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

    // 1. POČIATOČNÁ VIZUALIZÁCIA (Interaktívny mód)
    if (shm->mod == INTERAKTIVNY) {
        sem_wait(&shm->shm_mutex);  //ZAMKNUT, lebo sa ide menit zdielana pozicia chodca
        shm->aktualna_pozicia_chodca.riadok = aktualny_r;
        shm->aktualna_pozicia_chodca.stlpec = aktualny_s;
        sem_post(&shm->shm_mutex);  //ODOMKNUT Dáta v pamäti sú konzistentné

        // Signalizujeme klientovi, že sa zmenili dáta (pozícia chodca), su pripravene na vykreslenie
        sem_post(&shm->data_ready);
        usleep(200000); // Pauza, aby si používateľ stihol všimnúť štart
    }

    // 2. HLAVNÝ CYKLUS POHYBU
    // Chodec kráča, kým nedosiahne cieľ [0,0] ALEBO kým neminie limit K krokov.
    while ((aktualny_r != 0 || aktualny_s != 0) && pocet_krok < shm->K_max_kroky) {
        if (shm->stav == SIM_STOP_REQUESTED) {
            return;
        }
        // Náhodný výber smeru
        int smer = vyber_smeru(shm) % 4;
        int buduci_r = aktualny_r;
        int buduci_s = aktualny_s;

        // 3. TOROIDNÝ VÝPOČET BUDÚCEJ POZÍCIE
        // Modulo (%) zabezpečuje, že pri prejdení okraja sa objavím na opačnej strane.
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

        // 4. KONTROLA PREKÁŽOK A AKTUALIZÁCIA POZÍCIE
        // Chodec sa pohne len vtedy, ak na cieľových súradniciach nie je PREKAZKA.
        // Ak je tam prekážka, chodec zostáva na mieste (ale krok sa mu započíta).
        if (shm->svet[buduci_r][buduci_s] != PREKAZKA) {
            aktualny_r = buduci_r;
            aktualny_s = buduci_s;

            // Zápis aktuálnej pozície do SHM pod mutexom (pre interaktívne zobrazenie)
            sem_wait(&shm->shm_mutex);  //ZAMKNUTIE lebo ked sa pohne tak treba aktualizovat polohu
            shm->aktualna_pozicia_chodca.riadok = aktualny_r;
            shm->aktualna_pozicia_chodca.stlpec = aktualny_s;
            sem_post(&shm->shm_mutex);  //ODOMKNUTIE
        }

        pocet_krok++;

        // Ak som v interaktívnom móde, budím klienta po každom kroku a spomalíme beh
        if (shm->mod == INTERAKTIVNY) {
            sem_post(&shm->data_ready); //Po kazdom kroku dam klientovi vediet ze sa ma pohnut panacik
            usleep(100000);
        }
    }
    // 5. ZBER ŠTATISTÍK (Kritická sekcia)
    // Výsledky ukladám na súradnice start_r/start_s, pretože ma zaujíma
    // štatistika pre bod, odkiaľ chodec VYRAZIL.
    sem_wait(&shm->shm_mutex);  //ZAMKNUTIE aby si dve vlakna neprepisovali vysledky

    // Pripočítam celkový počet krokov (pre neskorší výpočet priemeru)
    shm->vysledky[start_r][start_s].avg_kroky += pocet_krok;

    // Ak chodec skončil cyklus preto, že je v cieli [0,0] (a nie kvôli limitu K)
    if (aktualny_r == 0 && aktualny_s == 0) {
        shm->vysledky[start_r][start_s].pravdepodobnost_dosiahnutia++;
    }
    sem_post(&shm->shm_mutex);  //ODOMKNUTIE
}

/**
 * @brief Inicializuje herný svet pre server.
 * * Načíta svet zo súboru pri opätovnom spustení, alebo vygeneruje nový náhodný svet
 * s prekážkami a overí jeho priechodnosť pomocou BFS.
 * * @param shm Smerník na zdieľanú pamäť.
 * @return true ak bol svet úspešne inicializovaný, false pri chybe alebo požiadavke na stop.
 */
bool inicializuj_svet_servera(ZdielaneData_t* shm) {
    // 1. VOĽBA: OPÄTOVNÉ SPUSTENIE (Načítanie)
    if (shm->opetovne_spustenie) {
        // Skúsim otvoriť súbor špecifikovaný v SHM a naplniť mapu
        if (!nacitaj_konfig_zo_suboru(shm)) {
            printf("[SERVER] Chyba: nepodarilo sa nacitat subor %s\n", shm->nazov_suboru);
            return false;
        }
        printf("[SERVER] Svet uspesne nacitany zo suboru\n");
        // 2. VOĽBA: NOVÁ SIMULÁCIA (Generovanie)
    } else {
        int pokusy_generovania = 0;
        // CYKLUS GENERUJ A TESTUJ:
        // Náhodné generovanie prekážok môže vytvoriť "mŕtve" mapy (odrezané časti).
        // Preto generujem svet opakovane, kým funkcia je_svet_validny (BFS) nepotvrdí priechodnosť.
        do {
            generuj_svet_s_prekazkami(shm, shm->pocet_prekazok);
            pokusy_generovania++;

            // Poistka: Ak používateľ počas generovania stlačí Q (Ukončiť), hneď vyskočím.
            if (shm->stav == SIM_STOP_REQUESTED) return false;
        } while (!je_svet_validny(shm));

        printf("[SERVER] Svet vygenerovany na %d. pokus.\n", pokusy_generovania);
    }
    return true; // Svet je pripravený na spustenie chodcov
}

/**
 * @brief Vykoná kompletnú sumárnu simuláciu pre všetky políčka sveta.
 * * Pre každú replikáciu prejde všetky políčka mriežky. Ak políčko nie je prekážka,
 * spustí z neho simuláciu náhodnej chôdze. Špeciálne ošetruje cieľový bod [0,0].
 * * @param shm Smerník na zdieľanú pamäť.
 */
void vykonaj_sumarnu_simulaciu(ZdielaneData_t* shm, int pipe_write_fd, int socket_fd, int* p_rezim_logovania) {
    // --- 1. RESET DÁT ---
    // Pred štartom vynulujem výsledky v zdieľanej pamäti pod mutexom,
    // aby som nezačínal so starými číslami z predchádzajúceho behu.
    sem_wait(&shm->shm_mutex);
    for(int r = 0; r < shm->riadky; r++) {
        for(int s = 0; s < shm->stlpece; s++) {
            shm->vysledky[r][s].avg_kroky = 0;
            shm->vysledky[r][s].pravdepodobnost_dosiahnutia = 0;
        }
    }
    sem_post(&shm->shm_mutex);

    // --- 2. HLAVNÝ CYKLUS REPLIKÁCIÍ ---
    for (int r_id = 0; r_id < shm->total_replikacie; r_id++) {

        // Kontrola, či klient počas výpočtu neposlal požiadavku na ukončenie (kláves Q)
        if (shm->stav == SIM_STOP_REQUESTED) {
            write(pipe_write_fd, "SERVER: Zastavujem vypocty na ziadost klienta.", 46);
            return;
        }
        char cmd;
        // KONTROLA SOCKETU (Asynchrónny príkaz)
        // MSG_DONTWAIT zabezpečí, že ak klient neposlal príkaz 'v', server nečaká a počíta ďalej.
        if (recv(socket_fd, &cmd, 1, MSG_DONTWAIT) > 0) {
            if (cmd == 'V' || cmd == 'v') {
                *p_rezim_logovania = !(*p_rezim_logovania);
                posli_log(pipe_write_fd, "SERVER: Režim globálneho logu bol prepnutý.");
            }
        }

        shm->aktualne_replikacie = r_id;

        // --- 3. PRECHOD CEZ CELÚ MRIEŽKU ---
        for (int riadok = 0; riadok < shm->riadky; riadok++) {
            for (int stlpec = 0; stlpec < shm->stlpece; stlpec++) {

                // Opakovaná kontrola zastavenia (dôležité pri miliónoch krokov)
                if (shm->stav == SIM_STOP_REQUESTED) {
                    // Log už posielam o úroveň vyššie, tu stačí return
                    return;
                }

                // ŠPECIÁLNY PRÍPAD: CIEĽ [0,0]
                // Cieľ sa nesimuluje – chodec je tam hneď, takže úspešnosť je 100% a kroky 0.
                if (riadok == 0 && stlpec == 0) {
                    if (r_id == 0) {
                        sem_wait(&shm->shm_mutex);
                        shm->vysledky[riadok][stlpec].pravdepodobnost_dosiahnutia = shm->total_replikacie;
                        shm->vysledky[riadok][stlpec].avg_kroky = 0;
                        sem_post(&shm->shm_mutex);
                    }
                    continue; // Simulácia chôdze pre cieľ sa nespúšťa
                }

                // Spustím simuláciu len pre voľné políčka (nie steny)
                if (shm->svet[riadok][stlpec] != PREKAZKA) {
                    simuluj_chodzu_z_policka(shm, riadok, stlpec);
                }
            }
        }

        // --- 4. VÝPOČET GLOBÁLNEJ ŠTATISTIKY PRE LOG ---
        // Tu server spočíta priemernú úspešnosť/kroky cez všetky voľné políčka naraz.
        double suma_hodnot = 0;
        int pocet_volnych = 0;
        for (int r = 0; r < shm->riadky; r++) {
            for (int s = 0; s < shm->stlpece; s++) {
                if (shm->svet[r][s] != PREKAZKA) {
                    if (*p_rezim_logovania == 0) { // Režim Percentá
                        suma_hodnot += ((double)shm->vysledky[r][s].pravdepodobnost_dosiahnutia / (r_id + 1));
                    } else { // Režim Kroky
                        suma_hodnot += ((double)shm->vysledky[r][s].avg_kroky / (r_id + 1));
                    }
                    pocet_volnych++;
                }
            }
        }

        //vyhnut sa deleniu 0
        double globalny_priemer = 0.0;
        if (pocet_volnych > 0) {
            globalny_priemer = suma_hodnot / pocet_volnych;
        } else {
            globalny_priemer = 0.0; // no free cells -> zero average
        }

        // --- 5. INFORMOVANIE KLIENTA ---
        // Po dokončení jednej replikácie: pošli notifikáciu klientovi a krátky log cez pipe
        // Aktualizujeme ukazovateľ aktualne_replikacie pod mutexom pre konzistenciu.
        sem_wait(&shm->shm_mutex); //ZAMKNUTIE
        // update completed replicas (1-based)
        shm->aktualne_replikacie = r_id + 1; // pocet prave dokoncenych replikacii
        sem_post(&shm->shm_mutex); //ODOMKNUTIE

        // Formátovanie správy podľa aktuálneho režimu (Percentá / Kroky)
        char msg[128];
        if (*p_rezim_logovania == 0) {
            // percentový režim -> zabezpečíme, že výsledok bude v rozmedzí 0..100
            double percent = globalny_priemer * 100.0;
            if (percent < 0.0) percent = 0.0;
            if (percent > 100.0) percent = 100.0;
            snprintf(msg, sizeof(msg), "SERVER: Repl. %d/%d - glob. uspesnost : %.1f%%",
                r_id + 1, shm->total_replikacie, percent);
        } else {
            // režim kroky - zobrazujeme priemerný počet krokov (bez percent)
            snprintf(msg, sizeof(msg), "SERVER: Repl. %d/%d - Glob. priem. krokov: %.1f",
                     r_id + 1, shm->total_replikacie, globalny_priemer);
        }
        posli_log(pipe_write_fd, msg);

        // Pošli krátku správu do pipe, aby sa klient mohol informovať aj textovo.
        if (pipe_write_fd >= 0) {
            char msg[128];
            snprintf(msg, sizeof(msg), "SERVER: Dokoncena replikacia %d/%d", r_id + 1, shm->total_replikacie);
            posli_log(pipe_write_fd, msg);
        }

        // Prebudíme klienta, aby vykreslil aktuálny stav (vrátane aktualne_replikacie)
        sem_post(&shm->data_ready);
        // usleep(100) uvoľní procesor, aby klient stihol prekresliť terminál bez lagovania
        //cim menej casu tak rychlejsie pojde program
        usleep(100);
    }
}

/**
 * @brief Hlavná riadiaca logika servera.
 * * Zabezpečuje čakanie na klienta, inicializáciu simulácie, spustenie zvoleného
 * módu (interaktívny/sumárny) a finálne uloženie výsledkov.
 * * @param shm Smerník na zdieľanú pamäť.
 */
void spusti_server(ZdielaneData_t* shm, int pipe_write_fd, int socket_fd) {
    int rezim_logovanie = 0; //0 = percenta 1 = kroky
    printf("[SERVER] Čakám na inicializáciu menu klientom...\n");

    // --- 1. ČAKANIE NA KLIENTA (Polling) ---
    // Server beží v nekonečnom cykle, kým v zdieľanej pamäti klient nenastaví SIM_INIT.
    while (shm->stav != SIM_INIT) {
        if (shm->stav == SIM_STOP_REQUESTED) return;
        usleep(10000);
    }
    // Inicializácia generátora náhodných čísel aktuálnym časom
    srand(time(NULL));

    posli_log(pipe_write_fd, "SERVER: inicializujem svet...");

    // --- 2. PRÍPRAVA MAPY ---
    // Zavolá sa funkcia, ktorá buď načíta súbor alebo spustí BFS generátor.
    if (!inicializuj_svet_servera(shm)) {
        if (shm->stav != SIM_STOP_REQUESTED) {
            posli_log(pipe_write_fd, "Server: Chyba pri inicializacii sveta!");
        } else {
            posli_log(pipe_write_fd, "Server: Inicializacia zrusena pouzivatelom.");
        }

        // Ak inicializácia zlyhá, musím korektne nastaviť stav a zobudiť klienta, aby nezamrzol.
        sem_wait(&shm->shm_mutex); // Zabezpečenie konzistencie stavu
        shm->stav = SIM_FINISHED;
        sem_post(&shm->shm_mutex);

        sem_post(&shm->data_ready);
        return;
    }

    // --- 3. SPUSTENIE SIMULÁCIE ---
    posli_log(pipe_write_fd, "Server: Svet pripraveny, startujem simulaciu.");
    shm->stav = SIM_RUNNING; // Oficiálne spúšťam výpočty

    if (shm->mod == INTERAKTIVNY) {
        // MOD 0: Sledujem jedného chodca zo stredu mapy
        posli_log(pipe_write_fd, "Server: Bezi interaktivny mod...");
        int start_r = shm->riadky / 2;
        int start_s = shm->stlpece / 2;
        shm->aktualne_replikacie = 0;

        simuluj_chodzu_z_policka(shm, start_r, start_s);
        usleep(30000); // Krátka pauza na doznenie vizualizácie
    } else {
        // MOD 1: Hromadný matematický výpočet (tisíce replikácií pre každé políčko)
        posli_log(pipe_write_fd, "Server: Bezi vypocet sumarneho modu...");
        vykonaj_sumarnu_simulaciu(shm, pipe_write_fd, socket_fd, &rezim_logovanie);
    }

    // --- 4. UKONČENIE A EXPORT ---
    // Ak simulácia dobehla do konca a nebola násilne prerušená:
    if (shm->stav != SIM_STOP_REQUESTED) {
        posli_log(pipe_write_fd, "Server: Ukladam vysledky do suboru...");
        uloz_vysledky_do_suboru(shm); // Automatické uloženie po úspešnom behu
        // Simulácia dobehla prirodzene -> nastavím FINISHED
        sem_wait(&shm->shm_mutex);
        shm->stav = SIM_FINISHED;
        sem_post(&shm->shm_mutex);
        posli_log(pipe_write_fd, "Server: Simulacia uspesne ukoncena.");
    } else {
        // Ak používateľ stlačil 'q' počas simulácie
        posli_log(pipe_write_fd, "Server: Simulacia prerusena pouzivatelom.");
    }

    // Finálny signál pre klienta (napr. na prekreslenie poslednej štatistiky)
    sem_post(&shm->data_ready);
    printf("[SERVER] Simulácia ukončená.\n");
}