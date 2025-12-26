//
// Created by damko on 16/12/2025.
//

#ifndef SEMESTRALKA_CLIENT_MENU_H
#define SEMESTRALKA_CLIENT_MENU_H
#include "ipc_shm.h"
/**
 * Hlavná funkcia pre zobrazenie menu a nastavenie parametrov simulácie.
 */
void zobraz_pociatocne_menu(ZdielaneData_t* shm);

/**
 * Pomocná funkcia na bezpečné načítanie celého čísla z konzoly v zadanom rozsahu.
 */
int nacitaj_cele_cislo(const char* otazka, int min, int max);

/**
 * Pomocná funkcia na načítanie názvu súboru s validáciou prípony .txt.
 */
void nacitaj_nazov_suboru(char* kam_ulozit, const char* text_vyzvy);

/**
 * Pomocná funkcia na načítanie a validáciu štyroch smerových pravdepodobností.
 */
void nacitaj_pravdepodobnosti(ZdielaneData_t* shm);

#endif //SEMESTRALKA_CLIENT_MENU_H