#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "headers/client_menu.h"

void zobraz_pociatocne_menu(ZdielaneData_t* shm) {
    printf("=== NASTAVENIA SIMULACIE ===\n");

    printf("Zadaj pocet replikacii: ");
    scanf("%d", &shm->total_replikacie);

    int volba_modu;
    printf("Vyber mod simulacie (0 - Interaktivny, 1 - Sumarny): ");
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
    char vstupe_text[20];
    char* smery[] = {"Hore", "Dole", "vlavo", "Vpravo"};

    do {
        suma = 0;
        printf("\nZadaj pravdepodobnosti pohybu (sucet musi byt 1.0, pouzivaj BODKU):\n");

        for (int i = 0; i < 4; i++) {
            while (1) {
                printf("Pravdepodobnost pre %s: ", smery[i]);

                // Načítame vstup ako text, aby sme mohli skontrolovať znaky
                if (scanf(" %19s", vstupe_text) != 1) {
                    continue;
                }

                // 1. KONTROLA ČIARKY: Ak používateľ zadal čiarku, nepustíme ho ďalej
                if (strchr(vstupe_text, ',') != NULL) {
                    printf("CHYBA: Nepouzivaj ciarku! Zadaj cislo s bodkou (napr. 0.25).\n");
                    continue;
                }

                // 2. PREVOD NA ČÍSLO: strtod vráti 0.0, ak text nie je číslo
                char* endptr;
                double hodnota = strtod(vstupe_text, &endptr);

                // Ak endptr ukazuje na začiatok, nebolo to číslo (napr. zadal "abc")
                if (vstupe_text == endptr) {
                    printf("CHYBA: Toto nie je platne cislo!\n");
                    continue;
                }

                // 3. VALIDÁCIA ROZSAHU: Musí byť v intervale [0, 1]
                if (hodnota < 0.0 || hodnota > 1.0) {
                    printf("CHYBA: Pravdepodobnost musi byt medzi 0.0 a 1.0!\n");
                    continue;
                }

                //priebezna kontrola
                if (suma + hodnota > 1.0001) { // Malá rezerva kvôli double presnosti
                    printf("CHYBA: Tato hodnota (%.2f) by prekrocila celkovy sucet 1.0 (aktualne mas %.2f)!\n", hodnota, suma);
                    //printf("Zadaj mensiu hodnotu pre %s.\n", smery[i]);
                    continue;
                }

                // Ak je všetko OK, uložím a idem na ďalší smer
                shm->pravdepodobnost[i] = hodnota;
                suma += hodnota;
                break;
            }
        }

        // 4. KONTROLA SÚČTU: Musí byť 1.0 (s malou toleranciou pre double)
        if (suma < 0.999 || suma > 1.001) {
            printf("CHYBA: Celkovy sucet je %.4f. Musi byt presne 1.0! Skus cele zadavanie znova.\n", suma);
        }

    } while (suma < 0.999 || suma > 1.001);

    // double suma;
    // bool zly_input = false;
    // do {
    //     suma = 0;
    //     printf("Zadaj pravdepodobnosti pohybu (sucet musi byt 1.0): \n");
    //     char* smery[] = {"Hore", "Dole", "vlavo", "Vpravo"};
    //
    //     for (int i = 0; i < 4; i++) {
    //         /* Keep prompting for the same direction until a valid number in [0,1]
    //            is entered. On invalid input we clear the input buffer and retry. */
    //         while (1) {
    //             if (zly_input == false) {
    //                 printf("Pravdepodobnost pre %s: ", smery[i]);
    //             } else {
    //                 i--;
    //                 printf("Pravdepodobnost pre %s: ", smery[i]);
    //             }
    //
    //             if (scanf(" %lf", &shm->pravdepodobnost[i]) != 1) {
    //                 printf("CHYBA: zadaj cislo s bodkou\n");
    //                 zly_input = true;
    //                 int c;
    //                 while ((c = getchar()) != '\n' && c != EOF);
    //
    //                 /* retry same index */
    //                 continue;
    //             }
    //
    //             /* validate range */
    //             if (shm->pravdepodobnost[i] < 0.0 || shm->pravdepodobnost[i] > 1.0) {
    //                 printf("CHYBA: pravdepodobnost musi byt v rozhrani [0,1]\n");
    //                 /* consume rest of line if any and retry */
    //                 int c;
    //                 while ((c = getchar()) != '\n' && c != EOF);
    //                 continue;
    //             }
    //
    //             /* accepted value */
    //             zly_input = false;
    //             suma += shm->pravdepodobnost[i];
    //             break;
    //         }
    //      }
    //     if (suma < 0.99 || suma > 1.01) {
    //         printf("Chyba: Sucet je %.4f. Musi byt 1.0. Skus znova\n", suma);
    //     }
    // } while (suma < 0.99 || suma > 1.01);

    shm->stav = SIM_INIT;
    printf("\n[MENU] Nastavenia pripravene, simulacia startuje \n");
}