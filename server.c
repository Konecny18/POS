#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "headers/ipc_shm.h"

int vyber_smeru(ZdielaneData_t* shm) {
    double r = (double)rand() / RAND_MAX;
    double kumulativna_suma = 0;

    for (int i = 0; i < 4; i++) {
        kumulativna_suma += shm->pravdepodobnost[i];
        if (r <= kumulativna_suma) {
            return i; //vrati 0(HORE) 1(DOLE) 2(Vlavo) 3(Vpravo)
        }
    }
    return 3; // Poistka pre zaokrúhľovacie chyby (Vpravo)
}

void generuj_svet_s_prekazkami(ZdielaneData_t* shm, int percento_prekazok) {
    for (int riadok = 0; riadok < MAX_ROWS; riadok++) {
        for (int stlpec = 0; stlpec < MAX_COLS; stlpec++) {
            //vynechanie stred mriezky aby chodec nezacinal v stene
            if (riadok == MAX_ROWS / 2 && stlpec == MAX_COLS / 2) {
                shm->svet[riadok][stlpec] = CHODEC;
                continue;
            }
            //nahodne rozhodnutie ci na policku bude prekazka
            if ((rand() % 100) < percento_prekazok) {
                shm->svet[riadok][stlpec] = PREKAZKA;
            } else {
                shm->svet[riadok][stlpec] = PRAZDNE;
            }
        }
    }
}

void simuluj_chodzu_z_policka(ZdielaneData_t* shm, int start_r, int start_s) {
    int aktualny_r = start_r;
    int aktualny_s = start_s;
    int pocet_krok = 0;

    //chodec ide kym nieje v cieli alebo neprekroci pocet K
    while ((aktualny_r != 0 || aktualny_s != 0) && pocet_krok < shm->K_max_kroky) {
        int smer = vyber_smeru(shm);
        int buduci_r = aktualny_r;
        int buduci_s = aktualny_s;

        //vypocet buducej pozicie
        if (smer == 0 && buduci_r > 0) {
            buduci_r--;
        } else if (smer == 1 && buduci_r < shm->riadky - 1) {
            buduci_r++; // Dole
        } else if (smer == 2 && buduci_s > 0) {
            buduci_s--;            // Vľavo
        } else if (smer == 3 && buduci_s < shm->stlpece - 1) {
            buduci_s++;
        }

        if (shm->svet[buduci_r][buduci_s] != PREKAZKA) {
            aktualny_r = buduci_r;
            aktualny_s = buduci_s;
        }

        pocet_krok++;

        //ak je INTERAKTIVNY mod musi signalizovat klientovi
        if (shm->mod == INTERAKTIVNY) {
            sem_wait(&shm->shm_mutex);
            shm->aktualna_pozicia_chodca.riadok = aktualny_r;
            shm->aktualna_pozicia_chodca.stlpec = aktualny_s;
            sem_post(&shm->shm_mutex);
            sem_post(&shm->data_ready);
            usleep(100000);
        }
    }
    // STATISTIKY
    sem_wait(&shm->shm_mutex);

    // PRIČÍTAŠ K ŠTARTOVACÍM SÚRADNICIAM, nie k aktuálnym (ktoré sú 0,0)
    shm->vysledky[start_r][start_s].avg_kroky += pocet_krok;

    // Ak dosiel do ciela [0,0], zvysi pocitadlo uspechov
    if (aktualny_r == 0 && aktualny_s == 0) {
        shm->vysledky[start_r][start_s].pravdepodobnost_dosiahnutia++;
    }
    sem_post(&shm->shm_mutex);
}

void spusti_server(ZdielaneData_t* shm) {
    srand(time(NULL));

    printf("[SERVER] Startujem slucku...\n");

    //generovanie sveta
    generuj_svet_s_prekazkami(shm, shm->pocet_prekazok);

    int kroky = 0;
    int LIMIT_KROKOV = shm->K_max_kroky;

    //nastavenia PC stavu
    shm->stav = SIM_RUNNING;
    shm->aktualna_pozicia_chodca.riadok = shm->riadky / 2;
    shm->aktualna_pozicia_chodca.stlpec = shm->stlpece / 2;

    //hlavny cyklus replikacii
    for (int r_id = 1; r_id <= shm->total_replikacie; r_id++) {
        shm->aktualne_replikacie = r_id;

        //pre kazde policko sveta
        for (int riadok = 0; riadok < shm->riadky; riadok++) {
            for (int stlpec = 0; stlpec < shm->stlpece; stlpec++) {
                //ak je policko prekazka alebo ciel, simulaciu odtial nepustam
                if (shm->svet[riadok][stlpec] == PREKAZKA || (riadok == 0 && stlpec == 0)) {
                    continue;
                }
                //spustim jednu cestu z chodca z tohto bodu
                simuluj_chodzu_z_policka(shm, riadok, stlpec);

                //ak niekdo ukoncil simulaciu cez menu
                if (shm->stav == SIM_EXIT) {
                    return;
                }
            }
        }
    }

    // printf("[SERVER] Simulacia zacina na pozicii [%d, %d]\n", shm->aktualna_pozicia_chodca.riadok, shm->aktualna_pozicia_chodca.stlpec);
    //
    // while (shm->stav == SIM_RUNNING) {
    //     //zamknutie
    //     sem_wait(&shm->shm_mutex);
    //
    //     if (kroky >= LIMIT_KROKOV) {
    //         shm->stav = SIM_FINISHED;
    //         sem_post(&shm->shm_mutex);
    //         sem_post(&shm->data_ready); // Posledný signál pre klienta, aby sa zobudil a skončil
    //         break;
    //     }
    //
    //     //kontrola prekazok
    //     Pozicia_t buduca_pozicia = shm->aktualna_pozicia_chodca;
    //     //logika pohybu
    //     int smer = rand() % 4; //0 hore, 1 dole, 2 vlavo, 3 vpravo
    //
    //     if (smer == 0 && buduca_pozicia.riadok > 0) {
    //         buduca_pozicia.riadok--;
    //     } else if (smer == 1 && buduca_pozicia.riadok < shm->riadky - 1) {
    //         buduca_pozicia.riadok++;
    //     } else if (smer == 2 && buduca_pozicia.stlpec > 0) {
    //         buduca_pozicia.stlpec--;
    //     } else if (smer == 3 && buduca_pozicia.stlpec < shm->stlpece - 1) {
    //         buduca_pozicia.stlpec++;
    //     }
    //
    //     //skontroluj ci na novej pozicii nieje prekazka
    //     if (shm->svet[buduca_pozicia.riadok][buduca_pozicia.stlpec] != PREKAZKA) {
    //         shm->aktualna_pozicia_chodca = buduca_pozicia;
    //         kroky++;
    //         printf("[SERVER] Chodec sa pohol na [%d, %d]\n]", shm->aktualna_pozicia_chodca.riadok, shm->aktualna_pozicia_chodca.stlpec);
    //     } else {
    //         printf("[SERVER] Naraz do prekazky chodec ostava na mieste\n");
    //         kroky++;
    //     }
        //odomknutie
        // sem_post(&shm->shm_mutex);
        //
        // //signalizacia klientovi, ze chodec sa pohol
        // sem_post(&shm->data_ready);
        //
        // //simulovanie casovy krok (napr 0.5 sekundy)
        // usleep(1000000);
    //}
    shm->stav = SIM_FINISHED;
    sem_post(&shm->data_ready); // Posledný signál pre klienta
    printf("[SERVER] Všetky replikácie dokončené.\n");
}

