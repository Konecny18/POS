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

    if (shm_ptr != NULL) {
        //znicenie semaforov
        sem_destroy(&shm_ptr->shm_mutex);
        sem_destroy(&shm_ptr->data_ready);

        //odpojenie pamate od procesu
        shmdt(shm_ptr);

        //uplne odstranime segment zo systemu
        int shm_id = shmget(key, sizeof(ZdielaneData_t), 0666);
        if (shm_id >= 0) {
            shmctl(shm_id, IPC_RMID, NULL);
        }

    }
}