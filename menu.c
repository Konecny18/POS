#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "headers/client_menu.h"

void zobraz_pociatocne_menu(ZdielaneData_t* shm) {
    printf("=== NASTAVENIA SIMULACIE ===\n");

    int volba_modu;
    do {
        printf("Vyber mod simulacie (0 - Interaktivny, 1 - Sumarny): ");
        if (scanf("%d", &volba_modu) != 1) { //ked zadam napr 1e tak mi to zoberie 1 e vymaze a idem zadavat dalsie udaje
            printf("CHYBA: zadaj cele cislo\n");
            while (getchar() != '\n'); //vycistenie buffera pretoze ked zadam 10,5 tak by mi zostalov bufferi 0.5
            volba_modu = -1;
            continue;
        }
        if (volba_modu < 0 || volba_modu > 1) {
            printf("CHYBA: zadaj cislo 0 alebo 1\n");
        }
    } while (volba_modu < 0 || volba_modu > 1);


    shm->mod = (volba_modu == 0) ? INTERAKTIVNY : SUMARNY;
    //shm->mod = (SimulaciaMod_t) volba_modu;

    if (volba_modu == SUMARNY) {
        do {
            printf("Zadaj pocet replikacii: ");
            if (scanf("%d", &shm->total_replikacie) != 1) {
                printf("CHYBA: zadaj cele cislo\n");
                while (getchar() != '\n');
                shm->total_replikacie = 0;
                continue;
            }
            if (shm->total_replikacie <= 0) {
                printf("CHYBA: Pocet replikacii musi byt vacsi ako 0\n");
            }
        } while (shm->total_replikacie <= 0);


    } else {
        shm->total_replikacie = 1;
    }

    //parametre simulacie a kontrola na nulu a pismena
    do {
        printf("Max. pocet krokov: ");
        if (scanf("%d", &shm->K_max_kroky) != 1) {
            printf("CHYBA: zadaj cele cislo\n");
            while (getchar() != '\n'); //vycistenie buffera
            shm->K_max_kroky = 0; //vynutenie opakovania
            continue;
        }
        if (shm->K_max_kroky <= 0) {
            printf("CHYBA: Pocet krokov musi byt aspon 1\n");
        }
    } while (shm->K_max_kroky <= 0);


    do {
        printf("Hustota prekazok (0-50%%): ");
        if (scanf("%d", &shm->pocet_prekazok) != 1) {
            printf("CHYBA: zadaj cele cislo\n");
            while (getchar() != '\n'); //vycistenie buffera
            shm->pocet_prekazok = -1;
            continue;
        }
        if (shm->pocet_prekazok < 0 || shm->pocet_prekazok > 50) {
            printf("CHYBA: Hustota prekazok musi byt v rozsahu 0-50%%\n");
        }
    } while (shm->pocet_prekazok < 0 || shm->pocet_prekazok > 50);


    //rozmery sveta s kontrolov limitov
    do {
        printf("Pocet riadkov (max %d): ", MAX_ROWS);
        if (scanf("%d", &shm->riadky) != 1) {
            printf("CHYBA: Zadaj cele cislo!\n");
            while(getchar() != '\n'); // Vyčistenie bufferu pri zadaní písmena
            shm->riadky = -1; // Vynútenie opakovania cyklu
            continue;
        }

        if (shm->riadky <= 0 || shm->riadky > MAX_ROWS) {
            printf("CHYBA: Pocet riadkov musi byt v rozsahu 1 az %d!\n", MAX_ROWS);
        }
    } while (shm->riadky <= 0 || shm->riadky > MAX_ROWS);

    do {
        printf("Pocet stlpcov (max %d): ", MAX_COLS);
        if (scanf("%d", &shm->stlpece) != 1) {
            printf("CHYBA: Zadaj cele cislo!\n");
            while(getchar() != '\n'); // Vyčistenie bufferu
            shm->stlpece = -1;
            continue;
        }

        if (shm->stlpece <= 0 || shm->stlpece > MAX_COLS) {
            printf("CHYBA: Pocet stlpcov musi byt v rozsahu 1 az %d!\n", MAX_COLS);
        }
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

    shm->stav = SIM_INIT;
    printf("\n[MENU] Nastavenia pripravene, simulacia startuje \n");
}