#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "headers/client_logic.h"

void vykresli_legendu(ZdielaneData_t* shm) {
    printf("\n------------------------------------------------------------\n");
    printf(" OVLÁDANIE:\n");
    printf(" [V] - Prepni zobrazenie (Priemer / Pravdepodobnosť)\n");
    printf(" [M] - Prepni mód (Interaktívny / Sumárny)\n");
    printf(" [Q] - Ukončiť simuláciu a návrat do menu\n");
    printf("------------------------------------------------------------\n");

    if (shm->stav == SIM_FINISHED) {
        printf(" STAV: Simulácia úspešne dokončená. Prezeráte si výsledky.\n");
    } else if (shm->stav == SIM_RUNNING) {
        printf(" STAV: Simulácia práve prebieha...\n");
    }
}


/**
 * Spracuje vstupy, ktoré ovplyvňujú len lokálne zobrazenie daného klienta.
 * Vráti true, ak došlo k zmene, ktorá vyžaduje prekreslenie.
 */
void prepni_lokalny_rezim_zobrazenia(int klavesa, RezimZobrazenia_t* p_rezim) {
    if (klavesa == 'v' || klavesa == 'V') {
        *p_rezim = (*p_rezim == ZOBRAZ_PRIEMER_KROKOV) ? ZOBRAZ_PRAVDEPODOBNOST_K : ZOBRAZ_PRIEMER_KROKOV;
    }
}

void* kontrola_klavestnice(void* arg) {
    VlaknoArgs_t* args = (VlaknoArgs_t*)arg;
    ZdielaneData_t* shm = args->shm;
    char c;
    while (shm->stav != SIM_STOP_REQUESTED && shm->stav != SIM_EXIT) {
        c = getchar();

        //ukoncenie pomoc stalecnia q
        if (c == 'q' || c == 'Q') {
            sem_wait(&shm->shm_mutex);
            shm->stav = SIM_STOP_REQUESTED;
            sem_post(&shm->shm_mutex);
            sem_post(&shm->data_ready);
            printf("[KLIENT] ukoncuje aplikaciu\n");
            break;
        }

        // 2. Prepnutie MODU (Zdieľané - prepne všetkým používateľom v SHM)
        if (c == 'm' || c == 'M') {
            sem_wait(&shm->shm_mutex);
            shm->mod = (shm->mod == INTERAKTIVNY) ? SUMARNY : INTERAKTIVNY;
            sem_post(&shm->shm_mutex);
            sem_post(&shm->data_ready); //prebud klienta pre okamzity update
        }

        //prepnutie typu sumaru (lokalne)
        if (c == 'v' || c == 'V') {
            prepni_lokalny_rezim_zobrazenia(c, args->p_rezim);
            sem_post(&shm->data_ready);
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

void vykresli_tabulku_statistik(ZdielaneData_t* shm, RezimZobrazenia_t rezim) {
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
    // printf("\n");
}

void spusti_klienta(ZdielaneData_t* shm) {
    //inicializacia na predvolenu hodnotu
    RezimZobrazenia_t aktualny_rezim = ZOBRAZ_PRIEMER_KROKOV;
    VlaknoArgs_t args;
    args.shm = shm;
    args.p_rezim = &aktualny_rezim;

    printf("[KLIENT] Spusteny, cakam na data...\n");

    pthread_t thread_id;
    pthread_create(&thread_id, NULL, kontrola_klavestnice, &args);
    //pthread_detach(thread_id);//vlakno pobezi nezavisle

    while (1) {
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
        } else if (shm->mod == SUMARNY){
            printf("\n >>>FINALNE VYSLEDKY<<<\n");
            vykresli_tabulku_statistik(shm, aktualny_rezim);
        } else {
            printf("[KLIENT] Sumarny mod: simulujem %d replikacii. Caka sa na vysledky...\n", shm->total_replikacie);
        }

        // TU VYPÍŠEME LEGENDU - bude pod tabuľkou/mapou
        vykresli_legendu(shm);

        if (shm->stav == SIM_EXIT || shm->stav == SIM_STOP_REQUESTED) {
            sem_post(&shm->shm_mutex);
            break; // Vyskočí z while a skončí proces klienta
        }

        //odomknutie
        sem_post(&shm->shm_mutex);
    }
    pthread_cancel(thread_id);
    printf("[KLIENT] Simulacia ukoncena\n");
    printf("[KLIENT] zadaj prikaz: \n");
}

