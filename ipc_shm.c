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

    return shm_ptr;
}

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

void shm_cleanup_semaphores(ZdielaneData_t* shm_ptr) {
    if (shm_ptr != NULL) {
        sem_destroy(&shm_ptr->shm_mutex);
        sem_destroy(&shm_ptr->data_ready);
    }
}