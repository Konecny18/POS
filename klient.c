#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#include "headers/client_logic.h"

/**
 * @brief Vlákno, ktoré číta správy zo serveru cez pipe a vypisuje notifikácie.
 *
 * Táto funkcia beží ako samostatné vlákno; blokujúco číta z `args->pipe_read_fd`
 * a vypisuje prijaté C-string správy od servera do konzoly. Ukončí sa keď
 * read vráti <= 0 (pipe zatvorené).
 *
 * @param arg Očakáva `VlaknoArgs_t*` obsahujúci `pipe_read_fd`.
 * @return NULL po ukončení vlákna.
 */
void* kontrola_pipe(void* arg) {
    // 1. PRETYPOVANIE ARGUMENTU
    // Premeníme všeobecný void pointer späť na štruktúru s mojimi dátami
    VlaknoArgs_t* args = (VlaknoArgs_t*)arg;
    char buffer[256];

    // 2. BLOKUJÚCE ČÍTANIE (Pipe)
    // Funkcia 'read' je blokujúca – to znamená, že toto vlákno tu bude "spať"
    // a nebude spotrebovávať žiadny výkon CPU, kým server niečo neodošle do pipe.
    ssize_t n;
    while ((n = read(args->pipe_read_fd, buffer, sizeof(buffer) - 1)) > 0) {
        // 3. OŠETRENIE REŤAZCA
        // Na koniec prijatých dát pridáme nulový znak '\0', aby sme vytvorili platný C-string
        buffer[n] = '\0';
        // 4. ULOŽENIE DO LOKÁLNEHO BUFFRA
        // Správu sa nesnažím hneď vypísať (printf), pretože by nám rozbila mriežku/tabuľku.
        // Namiesto toho ju uložím do 'lokalny_log_buffer', ktorý renderer (obsluz_vykreslovanie)
        // vykreslí na správnom mieste pod legendou pri ďalšom prekreslení.
        snprintf(args->lokalny_log_buffer, 256, "%s", buffer);

        // Poznámka: Toto vlákno zámerne nevyvoláva 'sem_post(&args->shm->data_ready)'.
        // Nechcem prekresľovať celú obrazovku len kvôli novému logu, počkáme na zmenu dát.
    }
    // Ak 'read' vráti 0 alebo menej, znamená to, že pipe bola zatvorená (server sa vypol)
    return NULL;
}


/**
 * @brief Vypíše legendu/ovládanie a krátky stav simulácie.
 *
 * Zobrazí informácie o klávesových skratkách pre klienta a stručný
 * stav simulácie (prebieha / dokončená). Používa sa pri každom prekreslení obrazovky.
 *
 * @param shm Ukazovateľ na zdieľanú pamäť obsahujúcu stav simulácie.
 */
void vykresli_legendu(ZdielaneData_t* shm, char* log) {
    // 1. ZOBRAZENIE LOGOV A REPLIKÁCIÍ
    // Podmienka kontroluje, či ide o hromadnú simuláciu (viac ako 1 replikácia).
    if (shm->total_replikacie > 1) {
        // Ak existuje nejaký textový log zo servera (prijatý cez pipe), vypíšem ho.
        // log[0] != '\n' zabezpečí, že nevypisujem prázdne riadky.
        if (log && log[0] != '\n') {
            printf(" | %s", log);
        }
        printf("\n");
    }

    // Rozlišujeme výpis podľa toho, či simulácia ešte beží alebo už skončila.
    if (shm->stav == SIM_FINISHED) {
        printf("\n------------------------------------------------------------\n");
        printf(" OVLÁDANIE:\n");
        printf(" [V] - Prepni zobrazenie (Priemer / Pravdepodobnosť)\n");
        printf(" [M] - Prepni mód (Interaktívny / Sumárny)\n");
        printf(" [Q] - Ukončiť simuláciu a návrat do menu\n");
        printf("------------------------------------------------------------\n");
        printf(" STAV: Simulácia úspešne dokončená. Prezeráte si výsledky.\n");
        printf("[KLIENT] zadaj prikaz: \n");
        // fflush(stdout) aby text "Zadaj príkaz" nezostal v buffri.
        fflush(stdout);
    } else if (shm->stav == SIM_RUNNING) {
        printf(" STAV: Simulácia práve prebieha...\n");
        printf(" (Môžete stláčať [M] pre zmenu módu alebo [Q] pre predčasné ukončenie)\n");
    }
}

void* kontrola_klavestnice(void* arg) {
    // 1. ROZBALENIE ARGUMENTOV
    // Z void pointera získame prístup k štruktúre, ktorá obsahuje SHM aj lokálny režim
    VlaknoArgs_t* args = (VlaknoArgs_t*)arg;
    ZdielaneData_t* shm = args->shm;
    int c; // use int for getchar() return to avoid narrowing issues

    // 2. HLAVNÝ CYKLUS VLÁKNA
    // Vlákno beží, kým nie je vyžiadané ukončenie simulácie (cez 'q' alebo zo strany servera)
    while (shm->stav != SIM_STOP_REQUESTED && shm->stav != SIM_EXIT) {
        // Program tu zastane a čaká, kým používateľ niečo nenapíše a nestlačí Enter
        c = getchar();

        // Ignoruj Enter a prázdne znaky, aby sa cyklus neprekrúcal zbytočne
        if (c == '\n' || c == '\r' || c == ' ') {
            continue;
        }

        //ukoncenie pomoc stalecnia q
        if (c == 'q' || c == 'Q') {
            // 1. Zmeníme stav OKAMŽITE bez čakania na mutex
            // Pri jednoduchom zápise do int v SHM to nespôsobí pád
            shm->stav = SIM_STOP_REQUESTED;

            // 2. Prebudíme hlavné vlákno klienta
            sem_post(&shm->data_ready);

            printf("[KLIENT] ukoncuje aplikaciu\n");
            return NULL;
        }

        // 2. Prepnutie MODU (Zdieľané - prepne všetkým používateľom v SHM)
        if (c == 'm' || c == 'M') {
            //printf("\033[H\033[J");
            sem_wait(&shm->shm_mutex);
            // Zmena módu v zdieľanej pamäti (všetci klienti uvidia zmenu)
            shm->mod = (shm->mod == INTERAKTIVNY) ? SUMARNY : INTERAKTIVNY;
            sem_post(&shm->shm_mutex);
            sem_post(&shm->data_ready); //prebud klienta pre okamzity update
        }

        //prepnutie typu sumaru (lokalne)
        if (c == 'v' || c == 'V') {
            //printf("\033[H\033[J");
            char cmd = 'V';
            // Ak simulácia beží, pošleme signál serveru cez socket
            if (shm->stav != SIM_FINISHED) {
                write(args->socket_fd, &cmd, 1);
            }

            // Zmena lokálneho režimu (vplýva len na tento terminál)
            *args->p_rezim = !(*args->p_rezim);
            // Signalizujeme zmenu pre prekreslenie
            sem_post(&shm->data_ready);
        }
    }
    return NULL;
}

/**
 * @brief Vykreslí mriežku sveta s aktuálnou pozíciou chodca v interaktívnom móde.
 *
 * Vykreslí znak 'C' na pozíciu chodca, '#' pre prekážky a '.' pre voľné políčka.
 * Používa `shm->aktualna_pozicia_chodca` a `shm->svet`.
 *
 * @param shm Ukazovateľ na zdieľanú pamäť obsahujúcu mapu a pozíciu chodca.
 */
void vykresli_mriezku_s_chodcom(ZdielaneData_t* shm) {
    printf("\n ---INTERAKTIVNA SIMULACIA---\n");
    for (int riadok = 0; riadok < shm->riadky; riadok++) {
        for (int stlpec = 0; stlpec < shm->stlpece; stlpec++) {
            if (shm->aktualna_pozicia_chodca.riadok == riadok && shm->aktualna_pozicia_chodca.stlpec == stlpec) {
                printf(" C ");
            } else if (shm->svet[riadok][stlpec] == PREKAZKA){
                printf(" # ");
            } else {
                printf(" . ");
            }
        }
        printf("\n");
    }
}

/**
 * @brief Vykreslí tabuľku štatistík v sumárnom móde podľa vybraného režimu.
 *
 * V závislosti od `rezim` vypíše buď priemerný počet krokov alebo percentuálnu
 * úspešnosť dosiahnutia cieľa pre každé políčko. Pre prekážky vypíše '###'.
 *
 * @param shm Ukazovateľ na zdieľanú pamäť obsahujúcu výsledky.
 * @param rezim Režim zobrazenia (ZOBRAZ_PRIEMER_KROKOV alebo ZOBRAZ_PRAVDEPODOBNOST_K).
 */
void vykresli_tabulku_statistik(ZdielaneData_t* shm, RezimZobrazenia_t rezim) {

    int completed_repl = 0;
    // 1. URČENIE POČTU DOKONČENÝCH REPLIKÁCIÍ (Menovateľ)
    // Ak je simulácia na konci, použijeme celkový plánovaný počet.
    if (shm->stav == SIM_FINISHED) {
        completed_repl = shm->total_replikacie;
    } else {
        // Ak ešte beží, vezmeme aktuálne číslo zo zdieľanej pamäte.
        // Server toto číslo zvyšuje po každej dokončenej replikácii.
        if (shm->aktualne_replikacie > 0) {
            completed_repl = shm->aktualne_replikacie; // already 1-based
        } else {
            completed_repl = 0;
        }
    }

    // OCHRANA PRED DELENÍM NULOU:
    // Ak ešte neprebehla ani jedna replikácia, nastavíme menovateľ (denom) na 1,
    // aby program nespadol pri výpočte priemeru (zobrazia sa nuly).
    int denom = (completed_repl > 0) ? completed_repl : 1;

    printf("\n ---SUMARNY MOD---\n");
    printf("Zobrazenie: %s\n\n", (rezim == ZOBRAZ_PRIEMER_KROKOV) ? "PRIEMERNY POCET KROKOV" : "PRAVDEPODOBNOST DOSIAHNUTIA (K)");

    // 2. PRECHÁDZANIE MATICE SVETA
    for (int riadok = 0; riadok < shm->riadky; riadok++) {
        for (int stlpec = 0; stlpec < shm->stlpece; stlpec++) {
            // Ak je na políčku prekážka, vypíšeme fixný znak
            if (shm->svet[riadok][stlpec] == PREKAZKA) {
                printf("| ### |");
            } else {
                if (rezim == ZOBRAZ_PRIEMER_KROKOV) {
                    // Vypočítame priemer: celkový súčet krokov / počet dokončených replikácií
                    double priemer = (double)shm->vysledky[riadok][stlpec].avg_kroky / denom;
                    printf("| %5.2f |", priemer);
                } else {
                    // Vypočítame úspešnosť v %: (úspešné dosiahnutia / počet replikácií) * 100
                    double uspesnost = ((double)shm->vysledky[riadok][stlpec].pravdepodobnost_dosiahnutia / denom * 100.0);

                    // OREZAŤ HODNOTY: Pre istotu udržíme percentá v rozsahu 0-100
                    if (uspesnost < 0.0) uspesnost = 0.0;
                    if (uspesnost > 100.0) uspesnost = 100.0;
                    printf("| %3.0f%%  |", uspesnost);
                }
            }
        }
        printf("\n");
    }

    // 4. INFORMAČNÝ RIADOK O PROGRESE
    // Zobrazuje sa pod tabuľkou, aby používateľ videl, ako ďaleko je simulácia.
    if (completed_repl > 0) {
        int display_done = completed_repl;
        // Ošetrenie, aby PROGRESS neukázal viac ako 100% (napr. pri dobehu vlákien)
        if (display_done > shm->total_replikacie) display_done = shm->total_replikacie;
        printf("\nPROGRESS: %d / %d\n", display_done, shm->total_replikacie);
    }
}

/**
 * @brief Rozhoduje o spôsobe vykreslenia dát na základe aktuálneho módu simulácie.
 * * Táto funkcia zabezpečuje vymazanie obrazovky a volanie príslušných podprogramov
 * pre interaktívny (mapa s chodcom) alebo sumárny (tabuľka štatistík) mód.
 * * @param shm Smerník na zdieľanú pamäť.
 * @param rezim Aktuálne zvolené typ zobrazenia v sumárnom móde (priemer/pravdepodobnosť).
 */
void obsluz_vykreslovanie(ZdielaneData_t* shm, RezimZobrazenia_t rezim, char* log) {
    // ANSI kód pre návrat kurzora na začiatok a vymazanie obrazovky
    printf("\033[H\033[J");

    if (shm->mod == SUMARNY) {
        printf("\n >>> FINALNE VYSLEDKY <<<\n");
        vykresli_tabulku_statistik(shm, rezim);
        //else if (shm->mod == SUMARNY && ((shm->aktualne_replikacie + 1) == shm->total_replikacie))
    } else if (shm->mod == INTERAKTIVNY) {
        vykresli_mriezku_s_chodcom(shm);
    } else {
        printf("[KLIENT] Simulujem %d replikacii. Caka sa na vysledky...\n", shm->total_replikacie);
    }

    vykresli_legendu(shm, log);
}

/**
 * @brief Hlavná riadiaca logika klientskej časti aplikácie.
 * * Inicializuje vlákno pre vstup z klávesnice a vstupuje do hlavného cyklu,
 * kde čaká na signály od servera (cez semafor data_ready). Po prijatí signálu
 * zabezpečí bezpečný prístup k dátam a ich vykreslenie.
 * * @param shm Smerník na zdieľanú pamäť.
 */
void spusti_klienta(ZdielaneData_t* shm, int pipe_read_fd, int socket_fd) {
    // 1. LOKÁLNA KONFIGURÁCIA KLIENTA
    // aktualny_rezim určuje, či vidíme priemerný počet krokov alebo % úspešnosť (prepína sa klávesom V)
    RezimZobrazenia_t aktualny_rezim = ZOBRAZ_PRIEMER_KROKOV;
    char log_buffer[256] = "";

    // 2. PRÍPRAVA VLÁKIEN
    pthread_t thread_id, pipe_thread_id;
    // Štruktúra args slúži na odovzdanie viacerých parametrov (SHM, deskriptory, lokálne stavy) do vlákien
    VlaknoArgs_t args = {
        .shm = shm,
        .p_rezim = &aktualny_rezim,
        .pipe_read_fd = pipe_read_fd,
        .socket_fd = socket_fd,
        .lokalny_log_buffer = log_buffer
    };

    printf("[KLIENT] Spusteny, cakam na data...\n");

    // Spustenie vlákna pre zachytávanie stlačených kláves (asynchrónny vstup)
    if (pthread_create(&thread_id, NULL, kontrola_klavestnice, &args) != 0) {
        perror("[KLIENT] Nepodarilo sa vytvorit vlakno pre klavesnicu");
        return;
    }
    // Spustenie vlákna pre čítanie textových logov zo servera cez pipe
    if (pthread_create(&pipe_thread_id, NULL, kontrola_pipe, &args) != 0) {
        perror("[KLIENT] Nepodarilo sa vytvorit vlakno pre pipe");
        pthread_cancel(thread_id);
        return;
    }

    // 3. SYNCHRONIZÁCIA PO PRIPOJENÍ
    // Ak sa pripájame k simulácii, ktorá už skončila, automaticky prepneme na sumárny mód
    if (shm->stav == SIM_FINISHED) {
        sem_wait(&shm->shm_mutex);
        shm->mod = SUMARNY;
        sem_post(&shm->shm_mutex);

        sem_post(&shm->data_ready);
    }

    // 4. ČAKANIE NA KONFIGURÁCIU SVETA
    // Krátka slučka (max 5s), ktorá čaká, kým server zapíše rozmery sveta (riadky/stĺpce).
    // Bez tohto by klient mohol spadnúť pri pokuse vykresliť maticu 0x0.
    int wait_loops = 0;
    while ((shm->riadky < 1 || shm->stlpece < 1) && wait_loops < 50) {
        // If server is not yet providing dimensions, sleep a bit and retry
        usleep(100000);
        wait_loops++;
    }

    if (shm->riadky < 1 || shm->stlpece < 1) {
        // Still missing configuration; print a message and continue with a safe empty render
        printf("[KLIENT] Varovanie: neznama konfiguracia sveta po cakaní, vykreslujem neskor\n");
    }

    // 5. ČAKANIE NA PRVÉ REÁLNE VÝSLEDKY
    // Prechádzame SHM a hľadáme, či server už vypočítal aspoň niečo nenulové.
    // Týmto zabránime "bliknutiu" prázdnej tabuľky hneď po spustení novej simulácie.
    int waited = 0;
    const int max_wait_iters = 50; // 50 * 100ms = 5s
    bool found_data = false;
    while (waited < max_wait_iters) {
        sem_wait(&shm->shm_mutex);
        if (shm->stav == SIM_FINISHED) {
            found_data = true;
            sem_post(&shm->shm_mutex);
            break;
        }

        // Kontrola, či je v matici výsledkov aspoň jedna nenulová hodnota
        for (int r = 0; r < shm->riadky && !found_data; r++) {
            for (int s = 0; s < shm->stlpece; s++) {
                if (shm->vysledky[r][s].avg_kroky != 0.0 || shm->vysledky[r][s].pravdepodobnost_dosiahnutia != 0.0) {
                    found_data = true;
                    break;
                }
            }
        }
        sem_post(&shm->shm_mutex);

        if (found_data) break;
        usleep(100000);
        waited++;
    }

    // Prvé (počiatočné) vykreslenie obrazovky
    sem_wait(&shm->shm_mutex);
    obsluz_vykreslovanie(shm, aktualny_rezim, log_buffer);
    sem_post(&shm->shm_mutex);

    // small sleep so user can notice the initial render (helps readability)
    usleep(200000);

    // 6. HLAVNÁ POLLOVACIA SLUČKA (RENDERER)
    // Keďže POSIX semafory nevedia zobudiť všetkých klientov naraz (broadcast),
    // používame polling – každých 150ms skontrolujeme, či sa v pamäti niečo zmenilo.
    int last_repl = -1;
    int last_stav = -1;
    int last_mod = -1;
    int last_zobrazenie = -1;
    while (1) {
        // Skúšame skonzumovať signál semaforu bez blokovania, aby hodnota nerástla do nekonečna
        if (sem_trywait(&shm->data_ready) == 0) {
            // consumed one pending notification (if any)
        }

        // Kontrola ukončenia: Ak server nastavil STOP alebo EXIT, klient končí slučku
        if (shm->stav == SIM_STOP_REQUESTED || shm->stav == SIM_EXIT) {
            break;
        }

        // Kritická sekcia: Čítame stavy zo zdieľanej pamäte pod mutexom
        sem_wait(&shm->shm_mutex);
        int cur_repl = shm->aktualne_replikacie;
        int cur_stav = shm->stav;
        int cur_mod = shm->mod;
        int cur_zobrazenie = aktualny_rezim;

        // PODMIENKA PREKRESLENIA:
        // Obrazovku prekreslíme len vtedy, ak sa niečo naozaj zmenilo (replikácia, mód, stav...)
        if (cur_repl != last_repl || cur_stav != last_stav ||
            cur_mod != last_mod || cur_zobrazenie != last_zobrazenie) {

            obsluz_vykreslovanie(shm, aktualny_rezim, log_buffer);
            // Aktualizujeme "posledné známe" stavy pre ďalšiu iteráciu
            last_repl = cur_repl;
            last_stav = cur_stav;
            last_mod = cur_mod;
            last_zobrazenie = cur_zobrazenie;
        }
        sem_post(&shm->shm_mutex);

        // Small sleep so output is human-readable and we don't spin CPU
        usleep(150000);
    }

    // 7. UKONČENIE A CLEANUP
    // Keď hlavná slučka skončí, násilne ukončíme pomocné vlákna a počkáme na ne
    pthread_cancel(thread_id);
    pthread_cancel(pipe_thread_id);

    pthread_join(pipe_thread_id, NULL);
    pthread_join(thread_id, NULL);
    printf("[KLIENT] Simulacia ukoncena.\n");
}