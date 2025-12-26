#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "headers/client_menu.h"

// Pomocná funkcia na bezpečné načítanie celého čísla v rozsahu
int nacitaj_cele_cislo(const char* otazka, int min, int max) {
    int hodnota;
    while (1) {
        printf("%s", otazka);
        if (scanf("%d", &hodnota) != 1) {
            printf("CHYBA: Zadaj cele cislo!\n");
            while (getchar() != '\n');
            continue;
        }
        if (hodnota < min || hodnota > max) {
            printf("CHYBA: Hodnota musi byt v rozsahu %d az %d!\n", min, max);
            continue;
        }
        return hodnota;
    }
}

// Pomocná funkcia na zadanie názvu súboru s kontrolou .txt
void nacitaj_nazov_suboru(char* kam_ulozit, const char* text_vyzvy) {
    while (1) {
        printf("%s (musi koncit na .txt): ", text_vyzvy);
        scanf("%255s", kam_ulozit);
        char *p = strstr(kam_ulozit, ".txt");
        if (p != NULL && strlen(p) == 4) {
            break;
        }
        printf("CHYBA: Neplatny nazov suboru!\n");
    }
}

// Pomocná funkcia pre pravdepodobnosti
void nacitaj_pravdepodobnosti(ZdielaneData_t* shm) {
    double suma;
    char vstup_text[20];
    char* smery[] = {"Hore", "Dole", "Vlavo", "Vpravo"};

    do {
        suma = 0;
        printf("\nZadaj pravdepodobnosti pohybu (sucet musi byt 1.0):\n");
        for (int i = 0; i < 4; i++) {
            while (1) {
                printf("Pravdepodobnost pre %s: ", smery[i]);
                scanf(" %19s", vstup_text);
                if (strchr(vstup_text, ',')) {
                    printf("CHYBA: Pouzivaj bodku!\n");
                    continue;
                }
                char* endptr;
                double hodnota = strtod(vstup_text, &endptr);
                if (vstup_text == endptr || hodnota < 0.0 || hodnota > 1.0) {
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

void zobraz_pociatocne_menu(ZdielaneData_t* shm) {
    printf("=== HLAVNE MENU ===\n");
    printf("1 - Nova nahodna simulacia\n");
    printf("2 - Opatovne spustenie (nacitat zo suboru)\n");

    int volba = nacitaj_cele_cislo("Tvoja volba: ", 1, 2);
    shm->opetovne_spustenie = (volba == 2);

    if (shm->opetovne_spustenie) {
        nacitaj_nazov_suboru(shm->nazov_suboru, "Zadaj nazov suboru pre NACITANIE");
        shm->mod = (nacitaj_cele_cislo("Mod (0-Interaktivny., 1-Sumarny.): ", 0, 1) == 0) ? INTERAKTIVNY : SUMARNY;
    } else {
        printf("\n=== NASTAVENIA NOVEJ SIMULACIE ===\n");

        shm->mod = (nacitaj_cele_cislo("Mod (0-Interaktivny., 1-Sumarny.): ", 0, 1) == 0) ? INTERAKTIVNY : SUMARNY;
        nacitaj_nazov_suboru(shm->nazov_suboru, "Zadaj nazov suboru pre ULOZENIE");

        if (shm->mod == SUMARNY) {
            shm->total_replikacie = nacitaj_cele_cislo("Zadaj pocet replikacii: ", 1, 1000000);
        } else {
            shm->total_replikacie = 1;
        }

        shm->K_max_kroky = nacitaj_cele_cislo("Max. pocet krokov: ", 1, 1000000);
        shm->pocet_prekazok = nacitaj_cele_cislo("Hustota prekazok (0-50%): ", 0, 50);
        shm->riadky = nacitaj_cele_cislo("Pocet riadkov: ", 1, MAX_ROWS);
        shm->stlpece = nacitaj_cele_cislo("Pocet stlpcov: ", 1, MAX_COLS);

        nacitaj_pravdepodobnosti(shm);
    }

    shm->stav = SIM_INIT;
    printf("\n[MENU] Nastavenia pripravene, simulacia startuje...\n");
}