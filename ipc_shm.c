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

#include "headers/ipc_shm.h"
#include "headers/common.h"

#define SHM_KEY 1234 //nahodny kluc pre identifikaciu pamate

/**
 * @brief Vytvorí a pripojí segment zdieľanej pamäte pre štruktúru ZdielaneData_t.
 *
 * Alokuje segment SHM pomocou `shmget`, pripojí ho pomocou `shmat`, vyčistí
 * obsah na nulu a inicializuje potrebné semafory (`shm_mutex`, `data_ready`).
 *
 * @param key Kľúč používaný pre `shmget`.
 * @return Ukazovateľ na pripojený segment typu `ZdielaneData_t` alebo NULL pri chybe.
 */
ZdielaneData_t* shm_create_and_attach(key_t key) {
    int shm_id;
    ZdielaneData_t* shm_ptr;

    //vytvorenie segmentu zdielanej pamate
    shm_id = shmget(key, sizeof(ZdielaneData_t), IPC_CREAT | 0666);
    if (shm_id < 0) {
        perror("shmget: Chyba pri vytvoreni segmentu SHM\n");
        return NULL;
    }

    //pripojenie segmentu k adresnemu priestoru procesu
    shm_ptr = (ZdielaneData_t*) shmat(shm_id, NULL, 0);
    if (shm_ptr == (void*) -1) {
        perror("shmat: Chyba pri pripojeni segmentu SHM");
        return NULL;
    }

    //vymaze vsetko co v pamati zostalo z predchadzajucich spusteni
    memset(shm_ptr, 0, sizeof(ZdielaneData_t));

    //inicializacia semaforov
    //shm_mutex: pshared=1 (zdielany medzi procesmi), hodnota=1 (odomknuty)
    if (sem_init(&shm_ptr->shm_mutex, 1, 1) == -1) {
        perror("sem_init mutex");
    }

    //data_ready: pshared=1 hodnota=0 (klient caka kym server nieco nevyrobi)
    if (sem_init(&shm_ptr->data_ready, 1, 0) != 0) {
        perror("sem_init data_ready");
    }

    // Ensure results area is clean initially
    shm_reset_results(shm_ptr);

    return shm_ptr;
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
    if (shm_ptr == NULL) {
        return;
    }
    // 1. Odpojenie pamäte od adresného priestoru tohto konkrétneho procesu
    if (shmdt(shm_ptr) == -1) {
        perror("shmdt");
    }

    // 2. Úplné odstránenie segmentu zo systému (len jeden proces by mal toto robiť)
    // Získame ID segmentu znova pre istotu
    int shm_id = shmget(key, 0, 0666);
    if (shm_id != -1) {
        // Kontrola, či už nie je označený na zmazanie nie je nutná,
        // ale IPC_RMID je bezpečné volať viackrát (vráti chybu ak už neexistuje)
        if (shmctl(shm_id, IPC_RMID, NULL) == -1) {
            // Ak to zlyhá, pravdepodobne to už iný proces odstránil (napr. server po ukončení)
            if (errno != EINVAL) perror("shmctl IPC_RMID");
        } else {
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
    if (shm_ptr != NULL) {
        sem_destroy(&shm_ptr->shm_mutex);
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

    // Clear result arrays under mutex protection if semaphore is initialized
    // Try to acquire mutex; if sem_wait fails we still attempt to clear without it
    if (sem_wait(&shm->shm_mutex) == 0) {
        for (int r = 0; r < shm->riadky; r++) {
            for (int s = 0; s < shm->stlpece; s++) {
                shm->vysledky[r][s].avg_kroky = 0.0;
                shm->vysledky[r][s].pravdepodobnost_dosiahnutia = 0.0;
                shm->vysledky[r][s].navstivene = false;
            }
        }
        shm->aktualne_replikacie = 0;
        // leave other persistent fields (nazov_suboru, pravdepodobnost, svet) intact
        sem_post(&shm->shm_mutex);
    } else {
        // fallback: zero memory region conservatively (only up to configured dimensions)
        for (int r = 0; r < shm->riadky; r++) {
            for (int s = 0; s < shm->stlpece; s++) {
                shm->vysledky[r][s].avg_kroky = 0.0;
                shm->vysledky[r][s].pravdepodobnost_dosiahnutia = 0.0;
                shm->vysledky[r][s].navstivene = false;
            }
        }
        shm->aktualne_replikacie = 0;
    }

    // Drain any leftover data_ready signals so client threads don't immediately wake
    while (sem_trywait(&shm->data_ready) == 0) {
        // loop until empty
    }
}
