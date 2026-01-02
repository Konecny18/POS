//struktura pre zdielanu pamat

#ifndef IPC_H
#define IPC_H

#include <semaphore.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/ipc.h>

#include "common.h"

/**
 * @brief Režim simulácie (interaktívny alebo sumárny).
 */
typedef enum {
    INTERAKTIVNY = 0,
    SUMARNY = 1
}SimulaciaMod_t;

/**
 * @brief Výsledky pre jedno políčko v mriežke.
 *
 * Uchováva kumulatívny súčet krokov (pre výpočet priemerov), počítadlo úspechov
 * a príznak, či bolo políčko navštívené pri BFS validácii sveta.
 */
typedef struct {
    double avg_kroky; //avg pocet krokov na dosiahnutie [0,0]
    double pravdepodobnost_dosiahnutia; //pravdepodobnost dosiahnutia [0,0] po max K(krokoch)
    bool navstivene; //zistuje ci je policko dosiahnutelne/navstivene(pre generovanie prekazok)
}Policko_vysledok;

/**
 * @brief Hlavná dátová štruktúra zdieľanej pamäte medzi serverom a klientom.
 *
 * Obsahuje semafory, nastavenia simulácie, stav, mapu sveta, výsledky, súborové
 * nastavenia a riadiace príznaky.
 */
typedef struct {
    //synch prostriedky
    sem_t shm_mutex; //binarny semafor na ochranu pred subeznym zapisom/citanim
    sem_t data_ready; //Semafor: server signalizuje ze su nove data pre klienta

    //nastavenia (definovane klientom na zaciatko)
    int riadky;
    int stlpece;
    int K_max_kroky;
    int total_replikacie;
    int pocet_prekazok;
    double pravdepodobnost[4]; //Pravdepodobnost pobyhu (hore, dole, vlavo, vpravo)

    //stav simulacie (aktualizovane serverom)
    volatile Simulacia_stav stav; //aktualny stav simulacie (INIT...)
    int aktualne_replikacie; //cislo replikacie

    SimulaciaMod_t mod;

    //data sveta a vysledky
    Typ_policka svet[MAX_ROWS][MAX_COLS];
    Policko_vysledok vysledky[MAX_ROWS][MAX_COLS];
    Pozicia_t aktualna_pozicia_chodca; //aktualna pozicia chodca

    //riadenie a ukoncenie
    bool client_ukoncenie; //ak klient zatvori aplikaciu (server sa tiez ma ukoncit)

    //praca zo suborom
    char nazov_suboru[256];
    bool opetovne_spustenie; //prikaz ze idem citat zo suboru
}ZdielaneData_t;

/**
 * @brief Vytvorí (ak treba) a pripojí segment zdieľanej pamäte.
 *
 * Alokuje segment SHM a inicializuje semafory a vráti ukazovateľ na pamäť.
 * @param key Kľúč pre segment.
 * @return Ukazovateľ na alokovanú a pripojenú `ZdielaneData_t` alebo NULL pri chybe.
 */
ZdielaneData_t* shm_create_and_attach(key_t key); // Vracia pointer, pretoze SHM alokuje pamäť dynamicky

/**
 * @brief Odpojí a odstráni segment zdieľanej pamäte (ak existuje).
 *
 * Odpojí lokálne pripojenie a pokúsi sa označiť segment pre odstránenie.
 *
 * @param shm_ptr Ukazovateľ na pripojenú zdieľanú pamäť (môže byť NULL).
 * @param key Kľúč segmentu SHM (používa sa pri označení pre odstránenie).
 */
void shm_detach_and_destroy(ZdielaneData_t* shm_ptr, key_t key);

/**
 * @brief Zničí semafory, ktoré boli inicializované v `shm_create_and_attach`.
 *
 * Odstraňuje/alokácie semaforov asociované s danou štruktúrou. Neoslobodzuje SHM.
 * @param shm_ptr Ukazovateľ na zdieľanú pamäť, z ktorej sa semafory zničia.
 */
void shm_cleanup_semaphores(ZdielaneData_t* shm_ptr);

/**
 * @brief Zresetuje polia výsledkov / per-run stavy v zdieľanej pamäti pred novou simuláciou.
 *
 * Vyčistí pole `vysledky`, nastaví `aktualne_replikacie` a ďalšie per-run príznaky na
 * predvolené hodnoty a vyprázdni semafor `data_ready` (ak obsahuje zostávajúce signály),
 * aby nový klient nezačal okamžite spracovávať staré notifikácie.
 *
 * @param shm Ukazovateľ na zdieľanú pamäť, ktorú treba resetovať.
 */
void shm_reset_results(ZdielaneData_t* shm);

#endif //IPC_H