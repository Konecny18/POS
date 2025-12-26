#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include "headers/ipc_shm.h"
#include <signal.h>
#include <string.h>

#include "headers/server_logic.h"
#include "headers/client_logic.h"
#include "headers/client_menu.h"

int main() {
    key_t key = 1234; //ten isty co som zadefinoval

    //vytvorenie a pripojenie zdielanej pamate
    ZdielaneData_t* shm = shm_create_and_attach(key);
    if (shm == NULL) {
        fprintf(stderr,"Nepodarilo sa vytvorit SHM.\n");
        return -1;
    }

    int koniec_programu = 0;
    while (!koniec_programu) {
        //resetovanie dat pred kazdou simulaciou
        memset(shm->vysledky, 0, sizeof(shm->vysledky));
        shm->stav = SIM_INIT;

        // 2. Volanie menu (Menu na konci nastaví shm->stav = SIM_INIT)
        zobraz_pociatocne_menu(shm);

        //rozdelenie procesov
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            shm_detach_and_destroy(shm, key);
            return -2;
        }

        if (pid == 0) {
            //PROCES KLIENT(dieta)
            spusti_klienta(shm);
            exit(0);
        } else {
            //PROCES SERVER(rodic)
            spusti_server(shm);

            //pockam kym dieta(klient) skonci
            wait(NULL);
        }

        while (getchar() != '\n'); // Vyčistí buffer
        // 3. Otázka na pokračovanie
        printf("\nChces spustit novu simulaciu? (1 - ANO, 0 - KONIEC): ");
        scanf("%d", &koniec_programu);
        koniec_programu = (koniec_programu == 0); // Ak zadá 0, koniec_programu bude 1 a cyklus skončí

        if (!koniec_programu) {
            // Resetujeme stav pre ďalšie kolo
            shm->stav = SIM_INIT;
        }
    }
    //upratovanie po simulacii
    printf("[SERVER] Upratujem SHM a koncim\n");
    shm_detach_and_destroy(shm, key);
    return 0;
}
