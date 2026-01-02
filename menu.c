#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "headers/client_menu.h"

/**
 * @brief Bezpečne načíta celé číslo z konzoly v zadanom rozsahu.
 *
 * Funkcia opakuje výzvu kým používateľ nezadá platné celé číslo v intervale
 * [min, max]. Pri neplatnom vstupe vyprázdni vstupný buffer a vypíše chybovú správu.
 *
 * @param otazka Text výzvy, ktorý sa vypíše používateľovi.
 * @param min Minimálna povolená hodnota (vrátane).
 * @param max Maximálna povolená hodnota (vrátane).
 * @return Načítaná celočíselná hodnota v rozsahu [min, max].
 */
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

/**
 * @brief Načíta názov súboru od používateľa s kontrolou prípony ".txt".
 *
 * Opakuje výzvu až kým používateľ nezadá reťazec končiaci na ".txt".
 * Výsledný názov je uložený do bufferu `kam_ulozit` (max 255 znakov).
 *
 * @param kam_ulozit Buffer kam sa uloží zadaný názov súboru (min veľkosť 256).
 * @param text_vyzvy Text výzvy, ktorý sa vypíše používateľovi.
 */
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

/**
 * @brief Načíta a validuje štyri smerové pravdepodobnosti z konzoly.
 *
 * Požiada používateľa o pravdepodobnosti pre smery "Hore", "Dole", "Vlavo", "Vpravo".
 * Hodnoty sa zadávajú ako desatinné čísla s bodkou (napr. 0.25). Funkcia zabezpečí,
 * že každá hodnota je v intervale [0.0, 1.0] a že súčet všetkých štyroch hodnôt je
 * približne 1.0 (povolená odchýlka 0.001).
 *
 * @param shm Ukazovateľ na zdieľanú pamäť, kde sú pravdepodobnosti uložené do `shm->pravdepodobnost`.
 */
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

/**
 * @brief Zobrazí počiatočné menu a načíta nastavenia simulácie od používateľa.
 *
 * Funkcia obsluhuje výber medzi novou náhodnou simuláciou a opätovným spustením
 * zo súboru. Podľa zvoleného módu vyzve používateľa na relevantné parametre
 * (počet replikácií, kroky, hustota prekážok, rozmery mapy, pravdepodobnosti).
 * Výsledok uloží do zdieľanej pamäte `shm` a nastaví `shm->stav = SIM_INIT`.
 *
 * @param shm Ukazovateľ na zdieľanú pamäť, kam sa uložia zvolené nastavenia.
 */
void zobraz_pociatocne_menu(ZdielaneData_t* shm) {
    printf("=== HLAVNE MENU ===\n");
    printf("Pocas behu aplikacie mozes pouzit nasledujuce prikazy:\n");
    printf("v + enter - Zmena priemeru krokov/pravdepodobnosti\n");
    printf("q + enter - Ukoncenie simulacie\n");
    printf("\n");
    printf("0 - Ukoncenie aplikacie\n");
    printf("1 - Nova nahodna simulacia\n");
    printf("2 - Opatovne spustenie (nacitat zo suboru)\n");

    int volba = nacitaj_cele_cislo("Tvoja volba: ",0, 2);
    shm->opetovne_spustenie = (volba == 2);

    if (volba == 0) {
        shm->stav = SIM_EXIT; // Nastavíme stav na EXIT, aby hlavný cyklus vedel, že končíme
        return; // Vrátime sa do main.c, kde cyklus skončí a uvoľní SHM
    } else if(shm->opetovne_spustenie) {
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



    // ak sme tu, nastavujeme simulacny stav na INIT a pokracujeme
    // Reset results to avoid leftover partial tables being displayed by client
    shm_reset_results(shm);
    shm->stav = SIM_INIT;
    printf("\n[MENU] Nastavenia pripravene, simulacia startuje...\n");
}