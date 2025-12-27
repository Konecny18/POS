//
// Created by damko on 16/12/2025.
//

#ifndef SEMESTRALKA_CLIENT_MENU_H
#define SEMESTRALKA_CLIENT_MENU_H

/**
 * @file client_menu.h
 * @brief Rozhranie pre funkcie zobrazujúce menu a načítanie vstupu od používateľa.
 *
 * Obsahuje prototypy funkcií, ktoré načítavajú parametre simulácie z konzoly
 * (bezpečné čítanie celých čísel, názvov súborov, a smerových pravdepodobností)
 * a funkciu pre zobrazenie počiatočného menu.
 */

#include "ipc_shm.h"

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
void zobraz_pociatocne_menu(ZdielaneData_t* shm);

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
int nacitaj_cele_cislo(const char* otazka, int min, int max);

/**
 * @brief Načíta názov súboru od používateľa s kontrolou prípony ".txt".
 *
 * Opakuje výzvu až kým používateľ nezadá reťazec končiaci na ".txt".
 * Výsledný názov je uložený do bufferu `kam_ulozit` (max 255 znakov).
 *
 * @param kam_ulozit Buffer kam sa uloží zadaný názov súboru (min veľkosť 256).
 * @param text_vyzvy Text výzvy, ktorý sa vypíše používateľovi.
 */
void nacitaj_nazov_suboru(char* kam_ulozit, const char* text_vyzvy);

/**
 * @brief Načíta a validuje štyri smerové pravdepodobnosti z konzoly.
 *
 * Požiada používateľa o pravdepodobnosti pre smery "Hore", "Dole", "Vlavo", "Vpravo".
 * Hodnoty sa zadávajú ako desatinné čísla s bodkou (napr. 0.25). Funkcia zabezpečí,
 * že každá hodnota je v intervale [0.0, 1.0] a že súčet všetkých štyroch hodnôt je
 * približne 1.0 (povolená odchýlka 0.001). Hodnoty sa ukladajú priamo do `shm->pravdepodobnost`.
 *
 * @param shm Ukazovateľ na zdieľanú pamäť, kde sú pravdepodobnosti uložené.
 */
void nacitaj_pravdepodobnosti(ZdielaneData_t* shm);

#endif //SEMESTRALKA_CLIENT_MENU_H