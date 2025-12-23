#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include "headers/ipc_shm.h"

//TODO pridat include pre logiku servera a klienta

int main() {
    key_t key = 1234; //ten isty co som zadefinoval

    //vytvorenie a pripojenie zdielanej pamate
    ZdielaneData_t* shm = shm_create_and_attach(key);
    if (shm == NULL) {
        fprintf(stderr,"Nepodarilo sa vytvorit SHM.\n");
        return -1;
    }

    //rozdelenie procesov
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        shm_detach_and_destroy(shm, key);
        return -2;
    }

    if (pid == 0) {
        //PROCES KLIENT(dieta)
        //TODO volanie pre GUI vykreslovanie
        printf("[KLIENT] Spusteny, cakam na data...\n");

        //zatial len simulacia prace
        sleep(2);

        printf("[KLIENT] Koncim\n");
        exit(0);
    } else {
        //PROCES SERVER(rodic)
        //TODO volanie logiky nahodnej pochodzky
        printf("[SERVER] Spusteny, zacinam simulaciu...\n");

        //zatial len simulacia prace
        sleep(3);

        //pockam kym dieta(klient) skonci
        wait(NULL);

        //upratovanie po simulacii
        printf("[SERVER] Upratujem SHM a koncim\n");
        shm_detach_and_destroy(shm, key);
    }
    return 0;
}