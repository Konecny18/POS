#include <stdio.h>
#include <stdlib.h>
#include "headers/client_logic.h"

void spusti_klienta(ZdielaneData_t* shm) {
    printf("[KLIENT] Spusteny, cakam na data...\n");

    while (/*shm->stav == SIM_RUNNING*/1) {
        //cakanie na signal
        //Klient zastavi a caka kym server neurobi sem_post
        sem_wait(&shm->data_ready);

        //zamknutie cesty
        sem_wait(&shm->shm_mutex);
        if (shm->stav == SIM_FINISHED || shm->stav == SIM_EXIT) {
            sem_post(&shm->shm_mutex);
            break; // Vyskočí z while a skončí proces klienta
        }

        //vymazanie obrazovky, aby simulacia nebezala pod seba
        printf("\003[H\033[J");
        printf("---SIMULACIA POHYBU CHODCA---\n");

        for (int riadok = 0; riadok < MAX_ROWS; riadok++) {
            for (int stlpec = 0; stlpec < MAX_COLS; stlpec++) {
                if (shm->aktualna_pozicia_chodca.riadok == riadok && shm->aktualna_pozicia_chodca.stlpec == stlpec) {
                    printf(" C ");
                } else {
                    printf(" . ");
                }
            }
            printf("\n");
        }

        //odomknutie
        sem_post(&shm->shm_mutex);
    }
    printf("[KLIENT] Simulacia ukoncena\n");
}