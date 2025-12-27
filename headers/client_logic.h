//
// Created by damko on 23/12/2025.
//

#ifndef SEMESTRALKA_CLIENT_LOGIC_H
#define SEMESTRALKA_CLIENT_LOGIC_H

/**
 * @file client_logic.h
 * @brief Deklarácie klientských funkcií a typov pre vizualizáciu simulácie.
 *
 * Tento header poskytuje prototypy funkcií používaných na zobrazenie výsledkov
 * simulácie a spracovanie lokálnych vstupov (klávesnica). Obsahuje tiež jednoduchý
 * enum a štruktúru pre lokálne režimy zobrazenia a odovzdávanie argumentov vlákna.
 */

#include "ipc_shm.h"



// Definícia režimov zobrazenia - len pre potreby klienta
/**
 * @brief Režimy, v ktorých môže klient zobrazovať sumárne výsledky.
 *
 * ZOBRAZ_PRIEMER_KROKOV: zobrazenie priemerného počtu krokov.
 * ZOBRAZ_PRAVDEPODOBNOST_K: zobrazenie percentuálnej úspešnosti dosiahnutia cieľa.
 */
typedef enum {
    ZOBRAZ_PRIEMER_KROKOV,
    ZOBRAZ_PRAVDEPODOBNOST_K
} RezimZobrazenia_t;

// Štruktúra pre vlákno, aby som mu odovzdal SHM aj lokálne nastavenie
/**
 * @brief Argumenty pre vlákno spracúvajúce klávesnicu.
 *
 * Obsahuje ukazovateľ na zdieľanú pamäť `shm` a ukazovateľ na lokálny
 * režim zobrazenia `p_rezim`, ktorý môže vlákno meniť (lokálne pre klienta).
 */
typedef struct {
    ZdielaneData_t* shm;
    RezimZobrazenia_t* p_rezim;
} VlaknoArgs_t;

/**
 * @brief Hlavná funkcia klienta, ktorá riadi prijímanie signálov a vykresľovanie.
 *
 * Spúšťa vlákno na čítanie klávesnice, čaká na semafór `data_ready` a volá
 * vykresľovacie funkcie podľa aktuálneho módu v `shm`.
 */
void spusti_klienta(ZdielaneData_t* shm);

/**
 * @brief Vláknová funkcia, ktorá beží na pozadí a spracúva stlačenia kláves.
 *
 * Očakáva argument typu `VlaknoArgs_t*`.
 */
void* kontrola_klavestnice(void* arg);

/**
 * @brief Vykreslí mapu sveta s aktuálnou pozíciou chodca (znak 'C').
 *
 * Používa `shm->aktualna_pozicia_chodca` a `shm->svet` na zostavenie ASCII mapy.
 */
void vykresli_mriezku_s_chodcom(ZdielaneData_t* shm);

/**
 * @brief Vykreslí finálnu tabuľku so štatistikami (priemerné kroky a percentá).
 *
 * Režim zobrazenia určuje parameter `rezim`.
 */
void vykresli_tabulku_statistik(ZdielaneData_t* shm, RezimZobrazenia_t rezim);

/**
 * @brief Vypíše ovládacie prvky (legendu) a stručný stav simulácie.
 *
 * Zobrazí nápovedu pre používateľa (klávesové skratky) a aktuálny stav.
 */
void vykresli_legendu(ZdielaneData_t* shm);

/**
 * @brief Prepne lokálny režim zobrazenia (priemer <-> pravdepodobnosť).
 *
 * Táto funkcia mení len lokálny režim `*p_rezim` a neovplyvňuje zdieľanú pamäť.
 * @param klavesa Stlačená klávesa (očakáva 'v' alebo 'V').
 * @param p_rezim Ukazovateľ na lokálny režim zobrazenia.
 */
void prepni_lokalny_rezim_zobrazenia(int klavesa, RezimZobrazenia_t* p_rezim);

/**
 * @brief Koordinuje, čo sa má vykresliť podľa aktuálneho módu simulácie.
 *
 * Volá `vykresli_mriezku_s_chodcom` v interaktívnom móde alebo
 * `vykresli_tabulku_statistik` v sumárnom móde a nakoniec vypíše legendu.
 */
void obsluz_vykreslovanie(ZdielaneData_t* shm, RezimZobrazenia_t rezim);

#endif // SEMESTRALKA_CLIENT_LOGIC_H

