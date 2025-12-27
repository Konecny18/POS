//
// Created by damko on 23/12/2025.
//

#ifndef SEMESTRALKA_CLIENT_LOGIC_H
#define SEMESTRALKA_CLIENT_LOGIC_H

#include "ipc_shm.h"



// Definícia režimov zobrazenia - len pre potreby klienta
typedef enum {
    ZOBRAZ_PRIEMER_KROKOV,
    ZOBRAZ_PRAVDEPODOBNOST_K
} RezimZobrazenia_t;

// Štruktúra pre vlákno, aby som mu odovzdal SHM aj lokálne nastavenie
typedef struct {
    ZdielaneData_t* shm;
    RezimZobrazenia_t* p_rezim;
} VlaknoArgs_t;

/**
 * Hlavná funkcia klienta, ktorá riadi prijímanie signálov a vykresľovanie.
 */
void spusti_klienta(ZdielaneData_t* shm);

/**
 * Vláknová funkcia, ktorá beží na pozadí a čaká na stlačenie klávesy 'q'.
 */
void* kontrola_klavestnice(void* arg);

/**
 * Vykreslí mapu sveta s aktuálnou pozíciou chodca (znak 'C').
 */
void vykresli_mriezku_s_chodcom(ZdielaneData_t* shm);

/**
 * Vykreslí finálnu tabuľku so štatistikami (priemerné kroky a percentá).
 */
void vykresli_tabulku_statistik(ZdielaneData_t* shm, RezimZobrazenia_t rezim);

#endif //SEMESTRALKA_CLIENT_LOGIC_H