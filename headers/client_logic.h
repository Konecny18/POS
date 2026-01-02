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
    int pipe_read_fd;
    int socket_fd;
    char* lokalny_log_buffer;
} VlaknoArgs_t;

/**
 * @brief Hlavná funkcia klienta, ktorá riadi prijímanie signálov a vykresľovanie.
 *
 * Spúšťa vlákno na čítanie klávesnice, čaká na semafór `data_ready` a volá
 * vykresľovacie funkcie podľa aktuálneho módu v `shm`.
 *
 * @param shm Ukazovateľ na zdieľanú pamäť `ZdielaneData_t` obsahujúcu výsledky a stav.
 * @param pipe_read_fd File descriptor pre čítanie prichádzajúcich správ od servera cez pipe.
 * @param socket_fd File descriptor socketu (alebo -1 ak nepoužité).
 */
void spusti_klienta(ZdielaneData_t* shm, int pipe_read_fd, int socket_fd);

/**
 * @brief Vláknová funkcia, ktorá beží na pozadí a spracúva stlačenia kláves.
 *
 * Očakáva argument typu `VlaknoArgs_t*`.
 * @param arg Ukazovateľ na `VlaknoArgs_t` obsahujúci `shm` a ďalšie hodnoty.
 * @return NULL po ukončení vlákna.
 */
void* kontrola_klavestnice(void* arg);

/**
 * @brief Vykreslí mapu sveta s aktuálnou pozíciou chodca (znak 'C').
 *
 * Používa `shm->aktualna_pozicia_chodca` a `shm->svet` na zostavenie ASCII mapy.
 * @param shm Ukazovateľ na zdieľanú pamäť so svetom a pozíciou.
 */
void vykresli_mriezku_s_chodcom(ZdielaneData_t* shm);

/**
 * @brief Vykreslí finálnu tabuľku so štatistikami (priemerné kroky a percentá).
 *
 * Režim zobrazenia určuje parameter `rezim`.
 * @param shm Ukazovateľ na zdieľanú pamäť s výsledkami.
 * @param rezim Režim zobrazenia (priemer krokov alebo pravdepodobnosti).
 */
void vykresli_tabulku_statistik(ZdielaneData_t* shm, RezimZobrazenia_t rezim);

/**
 * @brief Vypíše ovládacie prvky (legendu) a stručný stav simulácie.
 *
 * Zobrazí nápovedu pre používateľa (klávesové skratky) a aktuálny stav.
 * @param shm Ukazovateľ na zdieľanú pamäť, z ktorej sa číta stav simulácie.
 * @param log Pointer na buffer s lokálnym logom (môže byť NULL).
 */
void vykresli_legendu(ZdielaneData_t* shm, char* log);

// /**
//  * @brief Prepne lokálny režim zobrazenia (priemer <-> pravdepodobnosť).
//  *
//  * Táto funkcia mení len lokálny režim `*p_rezim` a neovplyvňuje zdieľanú pamäť.
//  * @param klavesa Stlačená klávesa (očakáva 'v' alebo 'V').
//  * @param p_rezim Ukazovateľ na lokálny režim zobrazenia.
//  */
// void prepni_lokalny_rezim_zobrazenia(int klavesa, RezimZobrazenia_t* p_rezim);

/**
 * @brief Koordinuje, čo sa má vykresliť podľa aktuálneho módu simulácie.
 *
 * Volá `vykresli_mriezku_s_chodcom` v interaktívnom móde alebo
 * `vykresli_tabulku_statistik` v sumárnom móde a nakoniec vypíše legendu.
 * @param shm Ukazovateľ na zdieľanú pamäť.
 * @param rezim Lokálny režim zobrazenia.
 * @param log Pointer na lokálny log buffer (môže byť NULL).
 */
void obsluz_vykreslovanie(ZdielaneData_t* shm, RezimZobrazenia_t rezim, char* log);

/**
 * @brief Vláknová funkcia, ktorá číta správy zo serveru cez pipe a zapisuje ich do lokálneho buffera.
 *
 * Táto funkcia je spustená v samostatnom vlákne a očakáva argument typu `VlaknoArgs_t*`.
 * Číta C-stringy z `pipe_read_fd` v `VlaknoArgs_t` a ukladá ich do `lokalny_log_buffer`.
 * Ukončí sa keď `read` vráti <= 0 (pipe zatvorená alebo chyba).
 *
 * @param arg Očakávaný typ `VlaknoArgs_t*`.
 * @return NULL po ukončení vlákna.
 */
void* kontrola_pipe(void* arg);

#endif // SEMESTRALKA_CLIENT_LOGIC_H
