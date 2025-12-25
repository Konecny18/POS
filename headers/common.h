#ifndef COMMON_H
#define COMMON_H

//rozmery sveta
#define MAX_ROWS 50
#define MAX_COLS 50
//stred sveta
#define CENTER_X 0
#define CENTER_Y 0

//typ policok
typedef enum {
    PRAZDNE, //prazdne policko, moze prejst chodec
    PREKAZKA, //prekazka, ak je svet s prekazkami
    CHODEC //aktualna pozicia chodca
}Typ_policka;

//typ sveta
typedef enum {
    SVET_BEZ_PREKAZOK, //svet bez prekazok
    SVET_S_PREKAZKAMI //svet s prekazkami
}Typ_sveta;

//struktura pre poziciu (policko)
typedef struct {
    int riadok;
    int stlpec;
}Pozicia_t;

//stav simulacie
typedef enum {
    SIM_INIT, //Simulacia inicializovana
    SIM_RUNNING, //prebieha
    SIM_STOP_REQUESTED, //pouzivatel ukoncuje
    SIM_PAUSED, //zastavena
    SIM_FINISHED, //skoncila
    SIM_EXIT //aplikacia sa ukoncuje
}Simulacia_stav;

#endif