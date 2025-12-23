#include <stdio.h>
#include "headers/client_menu.h"

void zobraz_pociatocne_menu(ZdielaneData_t* shm) {
    printf("=== NASTAVENIA SIMULACIE ===\n");

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

    //rozmery sveta s kontrolov limitov
    do {
        printf("Pocet riadkov (max %d): ", MAX_ROWS);
        scanf("%d", &shm->riadky);
    } while (shm->riadky <= 0 || shm->riadky > MAX_ROWS);

    do {
        printf("Pocet stlpcov (max %d): ", MAX_COLS);
        scanf("%d", &shm->stlpece);
    } while (shm->stlpece <= 0 || shm->stlpece > MAX_COLS);

    double suma;
    do {
        suma = 0;
        printf("Zadaj pravdepodobnosti pohybu (sucet musi byt 1.0): \n");
        char* smery[] = {"Hore", "Dole", "vlavo", "Vpravo"};
        for (int i = 0; i < 4; i++) {
            printf("Pravdepodobnost pre %s: ", smery[i]);
            scanf("%lf", &shm->pravdepodobnost[i]);
            suma += shm->pravdepodobnost[i];
        }
        if (suma < 0.99 || suma > 1.01) {
            printf("Chyba: Sucet je %.4f. Musi byt 1.0. Skus znova\n", suma);
        }
    } while (suma < 0.99 || suma > 1.01);

    shm->stav = SIM_INIT;
    printf("\n[MENU] Nastavenia pripravene, simulacia startuje \n");
}