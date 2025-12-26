#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "headers/client_menu.h"

void zobraz_pociatocne_menu(ZdielaneData_t* shm) {
    printf("=== HLAVNE MENU ===\n");
    printf("1 - Nova nahodna simulacia\n");
    printf("2 - Opatovne spustenie (nacitat zo suboru)\n");

    int hlavna_volba;
    do {
        printf("Tvoja volba: ");
        if (scanf("%d", &hlavna_volba) != 1) {
            while (getchar() != '\n');
            hlavna_volba = -1;
        }
    } while (hlavna_volba < 1 || hlavna_volba > 2);

    shm->opetovne_spustenie = (hlavna_volba == 2);

    if (shm->opetovne_spustenie) {
        // --- MOD PRE NACITANIE ZO SUBORU ---
        do {
            printf("Zadaj nazov suboru, z ktoreho sa maju nacitat data (musi koncit .txt): ");
            scanf("%255s", shm->nazov_suboru);
            char *p = strstr(shm->nazov_suboru, ".txt");
            if (p != NULL && strlen(p) == 4) break;
            printf("CHYBA: Zadaj platny nazov s .txt!\n");
        } while (1);

        printf("Vyber mod simulacie (0 - Interaktivny, 1 - Sumarny): ");
        int m;
        scanf("%d", &m);
        shm->mod = (m == 0) ? INTERAKTIVNY : SUMARNY;

    } else {
        // --- MOD PRE NOVU SIMULACIU (tvoj povodny kod) ---
        printf("\n=== NASTAVENIA NOVEJ SIMULACIE ===\n");

        int volba_modu;
        do {
            printf("Vyber mod simulacie (0 - Interaktivny, 1 - Sumarny): ");
            if (scanf("%d", &volba_modu) != 1) {
                printf("CHYBA: zadaj cele cislo\n");
                while (getchar() != '\n');
                volba_modu = -1;
                continue;
            }
            if (volba_modu < 0 || volba_modu > 1) {
                printf("CHYBA: zadaj cislo 0 alebo 1\n");
            }
        } while (volba_modu < 0 || volba_modu > 1);

        shm->mod = (volba_modu == 0) ? INTERAKTIVNY : SUMARNY;

        do {
            printf("Zadaj nazov suboru pre ULOZENIE (musi koncit na .txt): ");
            scanf("%255s", shm->nazov_suboru);
            char *p = strstr(shm->nazov_suboru, ".txt");
            if (p != NULL && strlen(p) == 4) break;
            printf("CHYBA: Zadaj platny nazov s .txt!\n");
        } while (1);

        if (shm->mod == SUMARNY) {
            do {
                printf("Zadaj pocet replikacii: ");
                if (scanf("%d", &shm->total_replikacie) != 1) {
                    printf("CHYBA: zadaj cele cislo\n");
                    while (getchar() != '\n');
                    shm->total_replikacie = 0;
                    continue;
                }
            } while (shm->total_replikacie <= 0);
        } else {
            shm->total_replikacie = 1;
        }

        do {
            printf("Max. pocet krokov: ");
            if (scanf("%d", &shm->K_max_kroky) != 1) {
                printf("CHYBA: zadaj cele cislo\n");
                while (getchar() != '\n');
                shm->K_max_kroky = 0;
                continue;
            }
        } while (shm->K_max_kroky <= 0);

        do {
            printf("Hustota prekazok (0-50%%): ");
            if (scanf("%d", &shm->pocet_prekazok) != 1) {
                printf("CHYBA: zadaj cele cislo\n");
                while (getchar() != '\n');
                shm->pocet_prekazok = -1;
                continue;
            }
        } while (shm->pocet_prekazok < 0 || shm->pocet_prekazok > 50);

        // Rozmery sveta
        do {
            printf("Pocet riadkov (max %d): ", MAX_ROWS);
            if (scanf("%d", &shm->riadky) != 1) {
                while(getchar() != '\n');
                shm->riadky = -1;
                continue;
            }
        } while (shm->riadky <= 0 || shm->riadky > MAX_ROWS);

        do {
            printf("Pocet stlpcov (max %d): ", MAX_COLS);
            if (scanf("%d", &shm->stlpece) != 1) {
                while(getchar() != '\n');
                shm->stlpece = -1;
                continue;
            }
        } while (shm->stlpece <= 0 || shm->stlpece > MAX_COLS);

        // Pravdepodobnosti
        double suma;
        char vstupe_text[20];
        char* smery[] = {"Hore", "Dole", "vlavo", "Vpravo"};

        do {
            suma = 0;
            printf("\nZadaj pravdepodobnosti pohybu (sucet musi byt 1.0):\n");
            for (int i = 0; i < 4; i++) {
                while (1) {
                    printf("Pravdepodobnost pre %s: ", smery[i]);
                    if (scanf(" %19s", vstupe_text) != 1) continue;
                    if (strchr(vstupe_text, ',') != NULL) {
                        printf("CHYBA: Nepouzivaj ciarku!\n");
                        continue;
                    }
                    char* endptr;
                    double hodnota = strtod(vstupe_text, &endptr);
                    if (vstupe_text == endptr || hodnota < 0.0 || hodnota > 1.0) {
                        printf("CHYBA: Neplatna hodnota!\n");
                        continue;
                    }
                    shm->pravdepodobnost[i] = hodnota;
                    suma += hodnota;
                    break;
                }
            }
        } while (suma < 0.999 || suma > 1.001);
    }

    shm->stav = SIM_INIT; // Signal pre server, aby odstartoval
    printf("\n[MENU] Nastavenia pripravene, simulacia startuje...\n");
}