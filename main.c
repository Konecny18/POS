#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include "headers/ipc_shm.h"
#include <signal.h>
#include <string.h>
#include <sys/shm.h>

#include "headers/server_logic.h"
#include "headers/client_logic.h"
#include "headers/client_menu.h"

int main() {
    key_t key = 1234;

    // 1. Vytvorenie SHM a inicializácia semaforov
    ZdielaneData_t* shm = shm_create_and_attach(key);
    if (shm == NULL) {
        fprintf(stderr, "Nepodarilo sa vytvorit SHM.\n");
        return -1;
    }

    int volba_pokracovat = 1;
    while (volba_pokracovat) {
        // RESET PARAMETROV PRE NOVÉ KOLO
        memset(shm->vysledky, 0, sizeof(shm->vysledky));
        shm->opetovne_spustenie = false; // Predvolene generujeme novú mapu
        shm->stav = SIM_FINISHED;        // Server zatiaľ čaká

        // 2. Volanie menu - TU sa nastaví SIM_INIT a parametre
        zobraz_pociatocne_menu(shm);

        // 3. Rozdelenie procesov
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            break;
        }

        if (pid == 0) {
            // --- PROCES KLIENT (Dieťa) ---
            spusti_klienta(shm);
            //odpojenie bloku od virtualnej adresnej mapy(nepotrebuje pristupovat ku zdielanej pamati)
            shmdt(shm);
            exit(0);
        } else {
            // --- PROCES SERVER (Rodič) ---
            spusti_server(shm);

            // Počkáme na smrť klienta, aby sa nám nepomiešali výpisy v konzole
            wait(NULL);
        }

        // 4. Otázka na pokračovanie
        printf("\nChces spustit uplne novu simulaciu? (1 - ANO, 0 - KONIEC): ");
        if (scanf("%d", &volba_pokracovat) != 1) {
            volba_pokracovat = 0;
        }
        while (getchar() != '\n'); // Vyčistenie bufferu

        if (volba_pokracovat == 0) {
            shm->stav = SIM_EXIT; // Nastavenie stavu pre finálne ukončenie
        }
    }

    // 5. Finálne upratovanie
    printf("\n[MAIN] Uvolnujem prostriedky a koncim aplikaciu...\n");

    // Zničenie semaforov (sem_destroy)
    shm_cleanup_semaphores(shm);

    // Odpojenie a odstránenie SHM zo systému (shmctl RMID)
    shm_detach_and_destroy(shm, key);

    return 0;
}
