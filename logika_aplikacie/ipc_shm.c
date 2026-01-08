//
// Created by damko on 16/12/2025.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <semaphore.h>
#include <errno.h>

#include "../headers/ipc_shm.h"
#include "../headers/common.h"

#define SHM_KEY 1234 //nahodny kluc pre identifikaciu pamate

/**
 * @brief Vytvorí a pripojí segment zdieľanej pamäte pre štruktúru ZdielaneData_t.
 *
 * Alokuje segment SHM pomocou `shmget`, pripojí ho pomocou `shmat`, vyčistí
 * obsah na nulu a inicializuje potrebné semafory (`shm_mutex`, `data_ready`) ak
 * ide o nový segment. Pri pripájaní k existujúcemu segmentu neprepisujeme jeho
 * obsah aby sme nestratili bežiacu simuláciu.
 *
 * @param key Kľúč používaný pre `shmget`.
 * @return Ukazovateľ na pripojený segment typu `ZdielaneData_t` alebo NULL pri chybe.
 */
ZdielaneData_t* shm_create_and_attach(key_t key) {
    int shm_id;
    ZdielaneData_t* shm_ptr;
    int novo_vytvorena = 0;

    // 1. POKUS O EXKLUZÍVNE VYTVORENIE
    // IPC_CREAT | IPC_EXCL spôsobí, že ak segment s daným kľúčom už existuje, shmget vráti chybu.
    // To povie: "Ty si ten, kto musí inicializovať semafory!"
    shm_id = shmget(key, sizeof(ZdielaneData_t), IPC_CREAT | IPC_EXCL | 0666);
    if (shm_id >= 0) {
        novo_vytvorena = 1; // Segment bol úspešne vytvorený práve teraz
    } else {
        if (errno == EEXIST) {
            // 2. PRIPOJENIE K EXISTUJÚCEMU SEGMENTU
            // Ak už segment existuje (EEXIST), získam jeho ID bez vytvárania.
            shm_id = shmget(key, sizeof(ZdielaneData_t), 0666);
            if (shm_id < 0) {
                perror("shmget (attach): Chyba pri ziskani existujuceho segmentu SHM");
                return NULL;
            }
            novo_vytvorena = 0; // Som len "klient", segment už niekto vytvoril preddomnou
        } else {
            perror("shmget: Chyba pri vytvoreni/otvoreni segmentu SHM");
            return NULL;
        }
    }

    // 3. MAPOVANIE DO ADRESNÉHO PRIESTORU
    // Funkcia shmat prepojí fyzickú zdieľanú pamäť s ukazovateľom v procese.
    shm_ptr = (ZdielaneData_t*) shmat(shm_id, NULL, 0);
    if (shm_ptr == (void*) -1) {
        //ak shmget vytvori segment a shmat (pripojenie do adresneho priestoru) zlyha
        //segment zostane vysiet v systeme tak tu ho vymazem
        if (novo_vytvorena) {
            shmctl(shm_id, IPC_RMID, NULL); // Ak som ho vytvoril, musíme ho aj zmazať
        }
        perror("shmat: Chyba pri pripojeni segmentu SHM");
        return NULL;
    }

    // 4. INICIALIZÁCIA (Len ak som tvorcova)
    //ak mam dva semafory a jeden sa inicializuje a druhy zlyha
    //funkcia by nemala vratit polofunkcny ukazovatel preto goto (kaskadovite cistenie)
    if (novo_vytvorena) {
        // Vyčistím pamäť od náhodných dát, ktoré tam mohli zostať z minulosti
        memset(shm_ptr, 0, sizeof(ZdielaneData_t));

        // INICIALIZÁCIA SEMAFOROV
        // Druhý parameter '1' hovorí, že semafor je zdieľaný MEDZI PROCESMI (pshared).

        // shm_mutex: Začína na 1 (odomknutý), slúži na vzájomné vylúčenie.
        if (sem_init(&shm_ptr->shm_mutex, 1, 1) == -1) {
            perror("sem_init mutex");
            goto error_cleanup;
        }

        // data_ready: Začína na 0 (klient spí), kým ho server nezobudí signálom.
        if (sem_init(&shm_ptr->data_ready, 1, 0) != 0) {
            perror("sem_init data_ready");
            sem_destroy(&shm_ptr->shm_mutex); // Zničíme ten prvý, čo sa podaril
            goto error_cleanup;
        }

        // Vyčistím plochu pre výsledky
        shm_reset_results(shm_ptr);
    } else {
        // Ak sa len pripájame, NESMIEME semafory znova inicializovať (sem_init by ich resetol).
    }

    return shm_ptr;

    error_cleanup:
        shmdt(shm_ptr);
        shmctl(shm_id, IPC_RMID, NULL);
        return NULL;
}

/**
 * @brief Odpojí a (ak je to možné) odstráni segment zdieľanej pamäte z OS.
 *
 * Funkcia bezpečne odpojí segment SHM (shmdt) a pokúsi sa nastaviť príznak
 * na odstránenie segmentu (IPC_RMID). Ak segment už neexistuje, ignoruje chybu.
 *
 * @param shm_ptr Ukazovateľ na pripojený segment.
 * @param key Kľúč, ktorý sa použije na získanie ID segmentu pri odstraňovaní.
 */
void shm_detach_and_destroy(ZdielaneData_t* shm_ptr, key_t key) {
    // Ak je ukazovateľ neplatný, nie je čo odpájať
    if (shm_ptr == NULL) {
        return;
    }

    // 1. ODPOJENIE (DETACH)
    // Funkcia shmdt povie systému: "Tento proces už s týmito dátami nebude pracovať".
    // Adresný priestor v mojom procese sa uvoľní, ale fyzická pamäť v systéme stále ostáva.
    if (shmdt(shm_ptr) == -1) {
        perror("shmdt");
    }

    // 2. ÚPLNÉ ODSTRÁNENIE (DESTROY)
    // V Linuxe zdieľaná pamäť prežíva aj ukončenie procesu.
    // Musím explicitne požiadať o jej zmazanie cez IPC_RMID.

    // Najprv získame ID segmentu na základe kľúča
    int shm_id = shmget(key, 0, 0666);
    if (shm_id != -1) {
        // IPC_RMID (Remove ID) neoznačuje okamžité vymazanie, ale "plánované zmazanie".
        // Segment sa reálne zmaže až vtedy, keď sa od neho odpojí POSLEDNÝ proces.
        if (shmctl(shm_id, IPC_RMID, NULL) == -1) {
            // EINVAL znamená, že segment už neexistuje (pravdepodobne ho už zmazal server)
            if (errno != EINVAL) perror("shmctl IPC_RMID");
        } else {
            // Toto sa vypíše len raz, keď úspešne požiadam o odstránenie
            printf("[IPC] Zdieľaná pamäť bola úplne odstránená zo systému.\n");
        }
    }
}

/**
 * @brief Zničí inicializované semafory v zdieľanej štruktúre.
 *
 * Zavolá `sem_destroy` pre `shm_mutex` a `data_ready` ak `shm_ptr` nie je NULL.
 *
 * @param shm_ptr Ukazovateľ na pripojený segment.
 */
void shm_cleanup_semaphores(ZdielaneData_t* shm_ptr) {
    // Kontrola, či vôbec máme platný ukazovateľ do pamäte
    if (shm_ptr != NULL) {
        // 1. ZNIČENIE MUTEXU
        // Funkcia informuje systém, že mutex už nebude používaný.
        // Ak by na mutexe niekto stále čakal (visel na sem_wait), správanie je nedefinované,
        // preto sa toto volá až vtedy, keď viem, že vlákna/procesy končia.
        sem_destroy(&shm_ptr->shm_mutex);
        // 2. ZNIČENIE SIGNALIZAČNÉHO SEMAFORU
        // Uvoľní prostriedky spojené so semaforom 'data_ready'.
        sem_destroy(&shm_ptr->data_ready);
    }
}

/**
 * @brief Reset per-run result fields in shared memory.
 *
 * This clears the aggregated results (vysledky) and per-run counters so that
 * starting a new simulation does not show leftover data from previous runs.
 * It also attempts to drain the `data_ready` semaphore to remove any stale
 * notifications.
 */
void shm_reset_results(ZdielaneData_t* shm) {
    if (shm == NULL) return;

    // --- 1. OCHRANA DÁT CEZ MUTEX ---
    // Skúsim zamknúť mutex, aby som mal exkluzívny prístup počas mazania.
    if (sem_wait(&shm->shm_mutex) == 0) {
        // PREMAZANIE MATICE VÝSLEDKOV
        for (int r = 0; r < MAX_ROWS; r++) {
            for (int s = 0; s < MAX_COLS; s++) {
                shm->vysledky[r][s].avg_kroky = 0.0;
                shm->vysledky[r][s].pravdepodobnost_dosiahnutia = 0.0;
                shm->vysledky[r][s].navstivene = false;
            }
        }
        // Vynulovanie počítadla replikácií a pozície chodca
        shm->aktualne_replikacie = 0;
        // reset current walker position to a safe default (0,0)
        shm->aktualna_pozicia_chodca.riadok = 0;
        shm->aktualna_pozicia_chodca.stlpec = 0;
        // ODOMKNUTIE: Ostatné procesy teraz vidia "čistý štít"
        sem_post(&shm->shm_mutex);
        // --- 2. ZÁLOŽNÝ PLÁN (FALLBACK) ---
    } else {
        // Ak sem_wait zlyhal (napr. semafor je poškodený), pre istotu dáta vynulujem
        // aj bez zámku, aby som nezostal visieť v nekonečnom čakaní.
        for (int r = 0; r < MAX_ROWS; r++) {
            for (int s = 0; s < MAX_COLS; s++) {
                shm->vysledky[r][s].avg_kroky = 0.0;
                shm->vysledky[r][s].pravdepodobnost_dosiahnutia = 0.0;
                shm->vysledky[r][s].navstivene = false;
            }
        }
        shm->aktualne_replikacie = 0;
        shm->aktualna_pozicia_chodca.riadok = 0;
        shm->aktualna_pozicia_chodca.stlpec = 0;
    }

    // --- 3. VYČISTENIE SIGNÁLOV (DRAIN) ---
    // Toto je kľúčová časť: Ak v semafore 'data_ready' zostali nejaké "pípnutia"
    // z minulej simulácie, klient by sa mohol začať okamžite budiť a kresliť nezmysly.
    // Cyklus while ich všetky "skonzumuje", kým semafor nie je prázdny (0).
    while (sem_trywait(&shm->data_ready) == 0) {
        // loop until empty
    }
}
