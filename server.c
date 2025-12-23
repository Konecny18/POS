#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "headers/ipc_shm.h"

void spusti_server(ZdielaneData_t* shm) {
    printf("[SERVER] Startujem slucku...\n");
    //inicializacia generatora
    srand(time(NULL));
    int kroky = 0;
    const int LIMIT_KROKOV = 10;

    //nastavenia PC stavu
    shm->riadky = SIM_RUNNING;
    shm->aktualna_pozicia_chodca.riadok = MAX_ROWS / 2;
    shm->aktualna_pozicia_chodca.stlpec = MAX_COLS / 2;

    printf("[SERVER] Simulacia zacina na pozicii [%d, %d]\n", shm->aktualna_pozicia_chodca.riadok, shm->aktualna_pozicia_chodca.stlpec);

    while (/*shm->stav == SIM_RUNNING*/1) {
        //zamknutie
        sem_wait(&shm->shm_mutex);

        if (kroky >= LIMIT_KROKOV) {
            shm->stav = SIM_FINISHED;
            sem_post(&shm->shm_mutex);
            sem_post(&shm->data_ready); // Posledný signál pre klienta, aby sa zobudil a skončil
            break;
        }

        //logika pohybu
        int smer = rand() % 4; //0 hore, 1 dole, 2 vlavo, 3 vpravo

        if (smer == 0 && shm->aktualna_pozicia_chodca.riadok > 0) {
            shm->aktualna_pozicia_chodca.riadok--;
        } else if (smer == 1 && shm->aktualna_pozicia_chodca.riadok < MAX_ROWS - 1) {
            shm->aktualna_pozicia_chodca.riadok++;
        } else if (smer == 2 && shm->aktualna_pozicia_chodca.stlpec > 0) {
            shm->aktualna_pozicia_chodca.stlpec--;
        } else if (smer == 3 && shm->aktualna_pozicia_chodca.stlpec < MAX_COLS - 1) {
            shm->aktualna_pozicia_chodca.stlpec++;
        }

        kroky++;
        //odomknutie
        sem_post(&shm->shm_mutex);

        //signalizacia klientovi, ze chodec sa pohol
        sem_post(&shm->data_ready);

        //simulovanie casovy krok (napr 0.5 sekundy)
        printf("[SERVER] Chodec sa pohol.\n");
        usleep(500000);
    }
    printf("[SERVER] Slucka skoncila, stav bol: %d\n", shm->stav);
}