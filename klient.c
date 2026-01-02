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
    VlaknoArgs_t* args = (VlaknoArgs_t*)arg;
    char buffer[256];

    //read je blokujuce vlakno tu bude spat kym server nieco neposle
    ssize_t n;
    while ((n = read(args->pipe_read_fd, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[n] = '\0';
        // ulozime do lokalneho log buffra, neprebudzujeme vykreslovanie na zaklade textovych logov
        snprintf(args->lokalny_log_buffer, 256, "%s", buffer);

        // predosle chovanie: sem_post(&args->shm->data_ready);
        // Uprava: log spravy nebudu automaticky prebudzovat renderer; renderer bude prebudzovany z praveho data_ready signalu od servera
    }
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
    // PRIDANÉ: Zobrazenie aktuálnej replikácie podľa bodu 10 zadania
    if (shm->total_replikacie > 1) {
        // int aktual = shm->aktualne_replikacie + 1;
        // if (aktual < 1) aktual = 0; // ochrana, ak este nie je nastavene
        // if (aktual > shm->total_replikacie) aktual = shm->total_replikacie;
        // printf("\n");
        // printf(" REPLIKÁCIA: %d / %d\n", aktual, shm->total_replikacie);
        if (log && log[0] != '\n') {
            printf(" | %s", log);
        }
        printf("\n");
    }
    if (shm->stav == SIM_FINISHED) {
        printf("\n------------------------------------------------------------\n");
        printf(" OVLÁDANIE:\n");
        printf(" [V] - Prepni zobrazenie (Priemer / Pravdepodobnosť)\n");
        printf(" [M] - Prepni mód (Interaktívny / Sumárny)\n");
        printf(" [Q] - Ukončiť simuláciu a návrat do menu\n");
        printf("------------------------------------------------------------\n");
        printf(" STAV: Simulácia úspešne dokončená. Prezeráte si výsledky.\n");
        printf("[KLIENT] zadaj prikaz: \n");
    } else if (shm->stav == SIM_RUNNING) {
        printf(" STAV: Simulácia práve prebieha...\n");
    }
}

/**
 * @brief Prepne lokálny režim zobrazenia pre jednotlivého klienta.
 *
 * Táto funkcia mení lokálny stav zobrazenia (priemer krokov vs. pravdepodobnosť)
 * bez ovplyvnenia ostatných klientov. Je určená pre spracovanie lokálnych klávesových
 * vstupov.
 *
 * @param klavesa Kód stlačenej klávesy.
 * @param p_rezim Ukazovateľ na premennú obsahujúcu aktuálny režim zobrazenia;
 *                 funkcia prepne hodnotu na druhý režim.
 */
// void prepni_lokalny_rezim_zobrazenia(int klavesa, RezimZobrazenia_t* p_rezim) {
//     if (klavesa == 'v' || klavesa == 'V') {
//         *p_rezim = (*p_rezim == ZOBRAZ_PRIEMER_KROKOV) ? ZOBRAZ_PRAVDEPODOBNOST_K : ZOBRAZ_PRIEMER_KROKOV;
//     }
// }

/**
 * @brief Vlákno pre asynchrónne snímanie vstupu z klávesnice.
 *
 * Spracováva klávesové skratky: ukončenie ('q'), prepnutie módu ('m') a
 * prepnutie lokálneho zobrazenia ('v'). Mení stav v zdieľanej pamäti pod mutexom
 * a notifikujte server/klienta cez semafóry.
 *
 * @param arg Ukazovateľ na štruktúru typu VlaknoArgs_t (obsahuje `shm` a `p_rezim`).
 * @return NULL pri ukončení vlákna.
 */
void* kontrola_klavestnice(void* arg) {
    VlaknoArgs_t* args = (VlaknoArgs_t*)arg;
    ZdielaneData_t* shm = args->shm;
    char c;
    while (shm->stav != SIM_STOP_REQUESTED && shm->stav != SIM_EXIT) {
        c = getchar();

        //ukoncenie pomoc stalecnia q
        if (c == 'q' || c == 'Q') {
            // sem_wait(&shm->shm_mutex);
            // shm->stav = SIM_STOP_REQUESTED;
            // sem_post(&shm->shm_mutex);
            //
            // sem_post(&shm->data_ready);

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
            printf("\033[H\033[J");
            sem_wait(&shm->shm_mutex);
            shm->mod = (shm->mod == INTERAKTIVNY) ? SUMARNY : INTERAKTIVNY;
            sem_post(&shm->shm_mutex);
            sem_post(&shm->data_ready); //prebud klienta pre okamzity update
        }

        //prepnutie typu sumaru (lokalne)
        if (c == 'v' || c == 'V') {
            printf("\033[H\033[J");
            char cmd = 'V';
            write(args->socket_fd, &cmd, 1);

            *args->p_rezim = !(*args->p_rezim);
            //prepni_lokalny_rezim_zobrazenia(c, args->p_rezim);
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
    //vypise tabulku a tie vypisi az na finalnej replikacii
    if ((shm->aktualne_replikacie + 1) == shm->total_replikacie) {
        printf("\n ---SUMARNY MOD---\n");
        printf("Zobrazenie: %s\n\n", (rezim == ZOBRAZ_PRIEMER_KROKOV) ? "PRIEMERNY POCET KROKOV" : "PRAVDEPODOBNOST DOSIAHNUTIA (K)");

        for (int riadok = 0; riadok < shm->riadky; riadok++) {
            for (int stlpec = 0; stlpec < shm->stlpece; stlpec++) {
                if (shm->svet[riadok][stlpec] == PREKAZKA) {
                    printf(" ### ");
                } else {
                    if (rezim == ZOBRAZ_PRIEMER_KROKOV) {
                        double priemer = (double)shm->vysledky[riadok][stlpec].avg_kroky / shm->total_replikacie;
                        printf("%5.2f ", priemer);
                    } else {
                        double uspesnost = ((double)shm->vysledky[riadok][stlpec].pravdepodobnost_dosiahnutia / shm->total_replikacie * 100);
                        printf("%3.0f%%  ", uspesnost);
                    }
                }
            }
            printf("\n");
        }
    }
    // printf("\n");
}

/**
 * @brief Rozhoduje o spôsobe vykreslenia dát na základe aktuálneho módu simulácie.
 * * Táto funkcia zabezpečuje vymazanie obrazovky a volanie príslušných podprogramov
 * pre interaktívny (mapa s chodcom) alebo sumárny (tabuľka štatistík) mód.
 * * @param shm Smerník na zdieľanú pamäť.
 * @param rezim Aktuálne zvolený typ zobrazenia v sumárnom móde (priemer/pravdepodobnosť).
 */
void obsluz_vykreslovanie(ZdielaneData_t* shm, RezimZobrazenia_t rezim, char* log) {
    // ANSI kód pre návrat kurzora na začiatok a vymazanie obrazovky
    printf("\033[H\033[J");

    if (shm->mod == INTERAKTIVNY) {
        vykresli_mriezku_s_chodcom(shm);
        //vypise finalne vysledky az na poslednej replikacii
    } else if (shm->mod == SUMARNY && ((shm->aktualne_replikacie + 1) == shm->total_replikacie)) {
        printf("\n >>> FINALNE VYSLEDKY <<<\n");
        vykresli_tabulku_statistik(shm, rezim);
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
    RezimZobrazenia_t aktualny_rezim = ZOBRAZ_PRIEMER_KROKOV;
    char log_buffer[256] = "";

    pthread_t thread_id, pipe_thread_id;
    //inicializacia args
    VlaknoArgs_t args = {
        .shm = shm,
        .p_rezim = &aktualny_rezim,
        .pipe_read_fd = pipe_read_fd,
        .socket_fd = socket_fd,
        .lokalny_log_buffer = log_buffer
    };


    printf("[KLIENT] Spusteny, cakam na data...\n");

    // Vytvorenie vlákna pre asynchrónne čítanie klávesnice
    if (pthread_create(&thread_id, NULL, kontrola_klavestnice, &args) != 0) {
        perror("[KLIENT] Nepodarilo sa vytvorit vlakno pre klavesnicu");
        return;
    }

    if (pthread_create(&pipe_thread_id, NULL, kontrola_pipe, &args) != 0) {
        perror("[KLIENT] Nepodarilo sa vytvorit vlakno pre pipe");
        pthread_cancel(thread_id);
        return;
    }

    while (/*shm->stav != SIM_STOP_REQUESTED && shm->stav != SIM_EXIT*/1) {
        // Čakanie na notifikáciu od servera, že sú dostupné nové dáta
        sem_wait(&shm->data_ready);

        // DEBUG: log SHM state to stderr to diagnose partial rendering
        // fprintf(stderr, "[CLIENT-DEBUG] woke: stav=%d mod=%d riadky=%d stlpece=%d aktualne_replikacie=%d total_replikacie=%d\n",
        //         shm->stav, shm->mod, shm->riadky, shm->stlpece, shm->aktualne_replikacie, shm->total_replikacie);

        // If server hasn't moved past INIT yet, ignore this wake-up (it's likely a log or stale signal)
        // if (shm->stav == SIM_INIT) {
        //     fprintf(stderr, "[CLIENT-DEBUG] Ignoring wake while in SIM_INIT\n");
        //     continue;
        // }
        //
        // // Guard against invalid dimensions (possible early/uninitialized state) - skip if invalid
        // if (shm->riadky < 1 || shm->riadky > MAX_ROWS || shm->stlpece < 1 || shm->stlpece > MAX_COLS) {
        //     fprintf(stderr, "[CLIENT-DEBUG] Invalid dimensions riadky=%d stlpece=%d - ignoring\n", shm->riadky, shm->stlpece);
        //     continue;
        // }

        //kvoli tomu ze vlakno mi tu vyselo ked som zadal vela udajov a predcasne som ukoncil simulaciu tak som bol zaseknuty
        if (shm->stav == SIM_STOP_REQUESTED || shm->stav == SIM_EXIT) {
            break; // Okamžite vyskočíme z cyklu vykresľovania
        }

        // Uzamknutie zdieľanej pamäte pred čítaním
        sem_wait(&shm->shm_mutex);

        // Vykreslenie aktuálneho stavu
        obsluz_vykreslovanie(shm, aktualny_rezim, log_buffer);

        int stav_po_vykresleni = shm->stav;

        // Kontrola, či neprišla požiadavka na ukončenie
        // if (shm->stav == SIM_EXIT || shm->stav == SIM_STOP_REQUESTED || shm->stav == SIM_FINISHED) {
        //     // Ak je simulácia dokončená, urobíme ešte jeden posledný render
        //     //obsluz_vykreslovanie(shm, aktualny_rezim);
        //     sem_post(&shm->shm_mutex);
        //     break;
        // }
        // Uvoľnenie zdieľanej pamäte
        sem_post(&shm->shm_mutex);

        if (stav_po_vykresleni == SIM_FINISHED) {

        }
    }

    // Korektné ukončenie pomocného vlákna a návrat
    pthread_cancel(thread_id);
    pthread_cancel(pipe_thread_id);

    pthread_join(pipe_thread_id, NULL);
    pthread_join(thread_id, NULL);
    printf("[KLIENT] Simulacia ukoncena.\n");
}