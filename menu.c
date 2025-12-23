#include <stdio.h>
#include "headers/client_menu.h"

void zobraz_pociatocne_menu(ZdielaneData_t* shm) {
    printf("=== NASTAVENIA SIMULACIE ===\n");

    //rozmery sveta s kontrolov limitov
    do {
        printf("Pocet riadkov (max %d): ", MAX_ROWS);
        scanf("%d", &shm->riadky);
    } while (shm->riadky <= 0 || shm->riadky > MAX_ROWS);

    do {
        printf("Pocet stlpcov (max %d): ", MAX_COLS);
        scanf("%d", &shm->stlpece);
    } while (shm->stlpece <= 0 || shm->stlpece > MAX_COLS);

    int volba_modu;
    printf("Vyber mod simulacie (0 - Interaktivny, 1 - Sumarny)");
    scanf("%d", &volba_modu);
    shm->mod = (volba_modu == 0) ? INTERAKTIVNY : SUMARNY;
    //shm->mod = (SimulaciaMod_t) volba_modu;

    //parametre simulacie
    printf("Max. pocet krokov: ");
    scanf("%d", &shm->K_max_kroky);

    printf("Hustota prekazok (0-50%%): ");
    scanf("%d", &shm->pocet_prekazok);

    shm->stav = SIM_INIT;
    printf("\n[MENU] Nastavenia pripravene, simulacia startuje \n");
}