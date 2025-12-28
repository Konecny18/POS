#ifndef COMMON_H
#define COMMON_H

/** @brief Maximálny počet riadkov v mriežke. */
#define MAX_ROWS 50
/** @brief Maximálny počet stĺpcov v mriežke. */
#define MAX_COLS 50

/**
 * @brief Typ políčka v mriežke.
 *
 * PRAZDNE: políčko, ktoré môže chodec prejsť.
 * PREKAZKA: neda sa prejst cez policko.
 * CHODEC: značí aktuálnu pozíciu chodca.
 */
typedef enum {
    PRAZDNE, //prazdne policko, moze prejst chodec
    PREKAZKA, //prekazka, ak je svet s prekazkami
    // CHODEC //aktualna pozicia chodca (unused) -- zakomentovane, nepouziva sa v projekte
}Typ_policka;

/**
 * @brief Typ sveta (bez prekážok alebo s prekážkami).
 */
/*
 * Tento typ sa momentalne nikde nepouziva v projekte. Zakomentoval som celu definiciu
 * aby som zachoval historiu a bolo jednoduche ju vratit, ak sa neskor ukaze, ze je potrebna.
 */

// typedef enum {
//     SVET_BEZ_PREKAZOK, //svet bez prekazok
//     SVET_S_PREKAZKAMI //svet s prekazkami
// }Typ_sveta;


/**
 * @brief Struktúra reprezentujúca pozíciu v mriežke.
 */
typedef struct {
    int riadok; /**< Riadok v mriežke (0..MAX_ROWS-1). */
    int stlpec; /**< Stĺpec v mriežke (0..MAX_COLS-1). */
}Pozicia_t;

/**
 * @brief Stav simulácie (riadi stav medzi klientom a serverom).
 */
typedef enum {
    SIM_INIT, //Simulacia inicializovana
    SIM_RUNNING, //prebieha
    SIM_STOP_REQUESTED, //pouzivatel ukoncuje
    // SIM_PAUSED, //zastavena (unused) - zakomentovane; ak je potrebne, vratit bez zmeny ciselnych hodnot
    SIM_FINISHED = 4, //skoncila (explicitne nastavenie, aby sa zachovali povodne cisla enumov)
    SIM_EXIT //aplikacia sa ukoncuje
}Simulacia_stav;

#endif