/**
 * @file main.c
 * @brief Hlavný vstupný bod aplikácie - vytvorí SHM, spustí menu, klienta a server.
 *
 * Program spúšťa interakciu: načítanie parametrov z menu, forkovanie procesu na
 * klienta a server, vykonanie simulácie a následné upratanie zdrojov (semafóry,
 * SHM). Po dokončení otázka používateľa, či spustiť novú simuláciu alebo ukončiť.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <sys/shm.h>

#include "headers/ipc_shm.h"
#include "headers/server_logic.h"
#include "headers/client_logic.h"
#include "headers/client_menu.h"

/**
 * @brief Hlavná funkcia aplikácie.
 *
 * Vytvorí a inicializuje zdieľanú pamäť, zobrazí menu pre nastavenie simulácie,
 * rozdelí proces na klienta a server (fork), čaká na dokončenie a zabezpečí
 * uvoľnenie všetkých prostriedkov pred ukončením programu.
 *
 * @return 0 pri úspechu, nenulový kód pri chybe.
 */
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

            // 4. Otázka na pokračovanie
            // Počkáme na smrť klienta, aby sa nám nepomiešali výpisy v konzole
            wait(NULL);
            //kontrola aby zadal pouzivatel bud 0 alebo 1
            do {
                printf("\nChces spustit uplne novu simulaciu? (1 - ANO, 0 - KONIEC): ");

                // Kontrola, či bolo zadané číslo
                if (scanf("%d", &volba_pokracovat) != 1) {
                    printf("Chyba: Musis zadat cislo (0 alebo 1)!\n");
                    volba_pokracovat = -1; // Nastavíme nevalidnú hodnotu, aby cyklus pokračoval
                }
                // Kontrola, či je číslo v povolenom rozsahu
                else if (volba_pokracovat != 0 && volba_pokracovat != 1) {
                    printf("Chyba: Zadaj bud 0 pre koniec alebo 1 pre novu simulaciu.\n");
                }

                // Vyčistenie bufferu (odstráni zvyšné znaky a \n)
                while (getchar() != '\n');

            } while (volba_pokracovat != 0 && volba_pokracovat != 1);
        }

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
