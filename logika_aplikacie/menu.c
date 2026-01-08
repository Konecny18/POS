#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../headers/client_menu.h"

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

        // 1. KONTROLA TYPU VSTUPU
        // scanf vráti počet úspešne načítaných položiek.
        // Ak používateľ zadá text (napr. "abc"), scanf vráti 0.
        if (scanf("%d", &hodnota) != 1) {
            printf("CHYBA: Zadaj cele cislo!\n");
            // VYČISTENIE BUFFERA:
            // Musím "prečítať a zahodiť" neplatné znaky (písmená), ktoré zostali v stdin.
            // Bez tohto by scanf v ďalšej iterácii opäť narazil na tie isté písmená.
            while (getchar() != '\n');
            continue;
        }
        // 2. KONTROLA ROZSAHU (Validácia)
        // Ak je číslo načítané správne, skontrolujem, či spĺňa moje limity.
        if (hodnota < min || hodnota > max) {
            printf("CHYBA: Hodnota musi byt v rozsahu %d az %d!\n", min, max);
            continue;
        }
        // Ak prešlo oboma kontrolami, vrátim výsledok
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
    char buffer[256];

    // Vyčistím vstupný buffer pred čítaním (pre istotu, ak tam zostal \n z predchádzajúceho scanf)
    // Toto je dôležité, inak fgets okamžite prečíta zvyšný Enter.
    int c;
    while ((c = getchar()) != '\n' && c != EOF);

    while (1) {
        printf("%s (musi koncit na .txt): ", text_vyzvy);

        // fgets načíta celý riadok
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            continue;
        }

        // Odstránim znak nového riadku '\n' z konca, ktorý tam fgets pridáva
        buffer[strcspn(buffer, "\n")] = '\0';

        // PRÍPAD 1: Používateľ stlačil iba Enter (prázdny reťazec)
        if (strlen(buffer) == 0) {
            kam_ulozit[0] = '\0'; // Vymažeme cieľový buffer
            printf("Ukladanie do suboru preskocene.\n");
            break;
        }

        // PRÍPAD 2: Používateľ niečo zadal, kontrolujeme príponu
        char *p = strstr(buffer, ".txt");
        if (p != NULL && strlen(p) == 4) {
            strcpy(kam_ulozit, buffer); // Skopírujeme overený názov
            break;
        }

        printf("CHYBA: Neplatny nazov suboru! Musi koncit na .txt\n");
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

    // Hlavný cyklus - opakuje sa, kým celkový súčet nie je 1.0
    do {
        suma = 0;
        printf("\nZadaj pravdepodobnosti pohybu (sucet musi byt 1.0):\n");
        for (int i = 0; i < 4; i++) {
            while (1) {
                printf("Pravdepodobnost pre %s: ", smery[i]);
                // Načítam vstup ako text, aby som ho mohl skontrolovať pred prevodom
                scanf(" %19s", vstup_text);
                // 1. KONTROLA DESATINNEJ ČIARKY
                if (strchr(vstup_text, ',')) {
                    printf("CHYBA: Pouzivaj bodku!\n");
                    continue;
                }
                // 2. PREVOD TEXTU NA DOUBLE (strtod je bezpečnejší ako atof)
                char* endptr;
                double hodnota = strtod(vstup_text, &endptr);
                // Ak endptr ukazuje na začiatok, nebolo načítané žiadne číslo
                if (vstup_text == endptr || hodnota < 0.0 || hodnota > 1.0) {
                    printf("CHYBA: Neplatna hodnota!\n");
                    continue;
                }
                shm->pravdepodobnost[i] = hodnota;
                suma += hodnota;
                break; // Úspešne načítaný jeden smer
            }
        }
        // 3. KONTROLA CELKOVEJ SUMY
        // Kvôli zaokrúhľovacím chybám typu double nekontrolujem "suma == 1.0",
        // ale malý tolerančný interval okolo jednotky.
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
int zobraz_pociatocne_menu(ZdielaneData_t* shm) {
    printf("=== HLAVNE MENU ===\n");
    //HLAVNE INFO
    printf("Pocas behu aplikacie mozes pouzit nasledujuce prikazy:\n");
    printf("v + enter - Zmena priemeru krokov/pravdepodobnosti\n");
    printf("m + enter - Zmena sumarneho rezimu na interaktivny\n");
    printf("q + enter - Ukoncenie simulacie\n");
    printf("\n");
    printf("0 - Ukoncenie aplikacie\n");
    printf("1 - Nova nahodna simulacia\n");
    printf("2 - Opatovne spustenie (nacitat zo suboru)\n");
    printf("3 - Pripojenie k beziacej simulacii\n");

    // Použitie mojej bezpečnej funkcie na načítanie voľby
    int volba = nacitaj_cele_cislo("Tvoja volba: ",0, 3);
    // Logický príznak, či budem generovať nový svet alebo čítať disk
    shm->opetovne_spustenie = (volba == 2);

    // --- SPRACOVANIE VOĽBY ---
    if (volba == 0) {
        // KONIEC: Nastavím stav, ktorý povie serveru aj klientovi, aby uvoľnili prostriedky
        shm->stav = SIM_EXIT; // Nastavíme stav na EXIT, aby hlavný cyklus vedel, že končíme
        return 0; // Vrátime sa do main.c, kde cyklus skončí a uvoľní SHM
    } else if(shm->opetovne_spustenie) {
        // REPLIKÁCIA ZO SÚBORU: Potrebujem len názov súboru a mód (Interaktívny/Sumárny)
        nacitaj_nazov_suboru(shm->nazov_suboru, "Zadaj nazov suboru pre NACITANIE");
        shm->mod = (nacitaj_cele_cislo("Mod (0-Interaktivny., 1-Sumarny.): ", 0, 1) == 0) ? INTERAKTIVNY : SUMARNY;
    } else if (volba == 3) {
        // PRIPOJENIE: Tu nič nenastavujem, klient len skočí do zobrazovacej slučky
        printf("\n [MENU] Pripajam sa k existujucej simulacii...\n");
        return 3;
    }else {
        // NOVÁ SIMULÁCIA: Kompletný proces konfigurácie
        printf("\n=== NASTAVENIA NOVEJ SIMULACIE ===\n");

        // 1. Výber módu a súboru pre budúce uloženie výsledkov
        shm->mod = (nacitaj_cele_cislo("Mod (0-Interaktivny., 1-Sumarny.): ", 0, 1) == 0) ? INTERAKTIVNY : SUMARNY;
        nacitaj_nazov_suboru(shm->nazov_suboru, "Zadaj nazov suboru pre ULOZENIE (ak nechces ukladat stlac ENTER)");

        // 2. Nastavenie rozsahu simulácie
        if (shm->mod == SUMARNY) {
            shm->total_replikacie = nacitaj_cele_cislo("Zadaj pocet replikacii: ", 1, 1000000);
        } else {
            shm->total_replikacie = 1;
        }

        // 3. Fyzické parametre sveta
        shm->K_max_kroky = nacitaj_cele_cislo("Max. pocet krokov: ", 1, 1000000);
        shm->pocet_prekazok = nacitaj_cele_cislo("Ak chces svet bez prekazok zadaj 0 inak zadaj percento prekazok (0-50%): ", 0, 50);
        shm->riadky = nacitaj_cele_cislo("Pocet riadkov: ", 1, MAX_ROWS);
        shm->stlpece = nacitaj_cele_cislo("Pocet stlpcov: ", 1, MAX_COLS);

        // 4. Smerové pravdepodobnosti
        nacitaj_pravdepodobnosti(shm);
    }

    // --- FINÁLNA PRÍPRAVA ---

    // Pred štartom vyčistím tabuľku výsledkov v SHM, aby klient nevidel staré dáta
    shm_reset_results(shm);
    // Nastavenie stavu na INIT – toto je signál pre server, že môže začať inicializovať svet
    shm->stav = SIM_INIT;
    printf("\n[MENU] Nastavenia pripravene, simulacia startuje...\n");
    return volba;
}