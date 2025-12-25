#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "headers/client_logic.h"

void* kontrola_klavestnice(void* arg) {
    ZdielaneData_t* shm = (ZdielaneData_t*)arg;
    char c;
    while (shm->stav == SIM_RUNNING) {
        c = getchar();
        if (c == 'q' || c == 'Q') {
            sem_wait(&shm->shm_mutex);
            shm->stav = SIM_STOP_REQUESTED;
            sem_post(&shm->shm_mutex);
            printf("[KLIENT] ukoncuje aplikaciu\n");
            break;
        }
    }
    return NULL;
}

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

void vykresli_tabulku_statistik(ZdielaneData_t* shm) {
    printf("\n ---SUMARNY MOD---\n");
    printf("Zobrazenie: Priemerny pocet krokov do [0,0]\n\n");

    for (int riadok = 0; riadok < shm->riadky; riadok++) {
        for (int stlpec = 0; stlpec < shm->stlpece; stlpec++) {
            if (shm->svet[riadok][stlpec] == PREKAZKA) {
                printf(" # ");
            } else {
                //vypocet priemeru
                double priemer = 0;
                if (shm->aktualne_replikacie > 0) {
                    priemer = (double)shm->vysledky[riadok][stlpec].avg_kroky / shm->aktualne_replikacie;
                }
                printf("%5.2f ", priemer);
            }
        }
        printf("\n");
    }
}

void spusti_klienta(ZdielaneData_t* shm) {
    printf("[KLIENT] Spusteny, cakam na data...\n");

    pthread_t thread_id;
    pthread_create(&thread_id, NULL, kontrola_klavestnice, shm);
    pthread_detach(thread_id);//vlakno pobezi nezavisle

    while (/*shm->stav == SIM_RUNNING || shm->stav == SIM_INIT*/1) {
        //cakanie na signal
        //Klient zastavi a caka kym server neurobi sem_post
        sem_wait(&shm->data_ready);
        //zamknutie cesty
        sem_wait(&shm->shm_mutex);

        //vymazanie obrazovky, aby simulacia nebezala pod seba
        printf("\033[H\033[J");

        if (shm->mod == INTERAKTIVNY) {
            // Tu zavoláš tvoju existujúcu logiku s cyklami pre mriežku a 'C'
            vykresli_mriezku_s_chodcom(shm);
        } else {
            // Tu zavoláš novú logiku pre výpis štatistík (priemery/pravdepodobnosť)
            vykresli_tabulku_statistik(shm);
        }

        if (shm->stav == SIM_FINISHED || shm->stav == SIM_EXIT || shm->stav == SIM_STOP_REQUESTED) {
            sem_post(&shm->shm_mutex);
            break; // Vyskočí z while a skončí proces klienta
        }

        //odomknutie
        sem_post(&shm->shm_mutex);
    }
    printf("[KLIENT] Simulacia ukoncena\n");
}

