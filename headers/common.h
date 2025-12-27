#ifndef COMMON_H
#define COMMON_H

/** @brief Maximálny počet riadkov v mriežke. */
#define MAX_ROWS 50
/** @brief Maximálny počet stĺpcov v mriežke. */
#define MAX_COLS 50
/** @brief Rezervované konštanty pre stred mapy (neaktuálne). */
#define CENTER_X 0
#define CENTER_Y 0

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
    CHODEC //aktualna pozicia chodca
}Typ_policka;

/**
 * @brief Typ sveta (bez prekážok alebo s prekážkami).
 */
//TODO neviem ci to realne pouzivam (asi pouzivam iba jedno)
typedef enum {
    SVET_BEZ_PREKAZOK, //svet bez prekazok
    SVET_S_PREKAZKAMI //svet s prekazkami
}Typ_sveta;

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
    SIM_PAUSED, //zastavena
    SIM_FINISHED, //skoncila
    SIM_EXIT //aplikacia sa ukoncuje
}Simulacia_stav;

#endif