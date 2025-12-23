//struktura pre zdielanu pamat

#ifndef IPC_H
#define IPC_H

#include <semaphore.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/ipc.h>

#include "common.h"

//struktura pre uchovanie vysledkov pre jedno policko
typedef struct {
    double avg_kroky; //avg pocet krokov na dosiahnutie [0,0]
    double pravdepodobnost_dosiahnutia; //pravdepodobnost dosiahnutia [0,0] po max K(krokoch)
    bool navstivene; //zistuje ci je policko dosiahnutelne/navstivene(pre generovanie prekazok)
}Policko_vysledok;

typedef struct {
    //synch prostriedky
    sem_t shm_mutex; //binarny semafor na ochranu pred subeznym zapisom/citanim
    sem_t data_ready; //Semafor: server signalizuje ze su nove data pre klienta

    //nastavenia (definovane klientom na zaciatko)
    int riadky;
    int stlpece;
    int K_max_kroky;
    int total_replikacie;
    double pravdepodobnost[4]; //Pravdepodobnost pobyhu (hore, dole, vlavo, vpravo)

    //stav simulacie (aktualizovane serverom)
    Simulacia_stav stav; //aktualny stav simulacie (INIT...)
    int aktualne_replikacie; //cislo replikacie

    //data sveta a vysledky
    Typ_policka svet[MAX_ROWS][MAX_COLS];
    Policko_vysledok vysledky[MAX_ROWS][MAX_COLS];
    Pozicia_t aktualna_pozicia_chodca; //aktualna pozicia chodca

    //riadenie a ukoncenie
    bool client_ukoncenie; //ak klient zatvori aplikaciu (server sa tiez ma ukoncit)
}ZdielaneData_t;

//fukcia pre pracu so zdielanov pamatou
//implemetovane v samotnom c subore
ZdielaneData_t* shm_create_and_attach(key_t key); // Vracia pointer, pretože SHM alokuje pamäť dynamicky
void shm_detach_and_destroy(ZdielaneData_t* shm_ptr, key_t key);
void shm_cleanup_semaphores(ZdielaneData_t* shm_ptr);

#endif //IPC_H