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
    int pipe_fd[2];

    int old_shmid = shmget(key, sizeof(ZdielaneData_t), 0666);
    if (old_shmid != -1) {
        // Ak existuje, odstránime ju, aby sme začali s čistým štítom
        shmctl(old_shmid, IPC_RMID, NULL);
        printf("[MAIN] Vyčistená stará zdieľaná pamäť po nečakanom ukončení.\n");
    }

    // 1. Vytvorenie SHM a inicializácia semaforov
    ZdielaneData_t* shm = shm_create_and_attach(key);
    if (shm == NULL) {
        fprintf(stderr, "Nepodarilo sa vytvorit SHM.\n");
        return -1;
    }

    int volba_pokracovat = 1;
    while (volba_pokracovat) {
        // RESET PARAMETROV PRE NOVÉ KOLO
        // Reset per-run fields so old results are not visible when starting a new simulation
        shm_reset_results(shm);
        shm->opetovne_spustenie = false; // Predvolene generujeme novú mapu
        shm->stav = SIM_FINISHED;        // Server zatiaľ čaká

        if (pipe(pipe_fd) == -1) {
            perror("pipe");
            return -1;
        }

        // 2. Volanie menu - TU sa nastaví SIM_INIT a parametre
        zobraz_pociatocne_menu(shm);

        // Ak používateľ zvolil ukončenie v menu (volba 0), ukončíme hlavnú slučku a vykonáme upratovanie
        if (shm->stav == SIM_EXIT) {
            printf("[MAIN] Pouzivatel zvolil ukoncenie aplikacie v menu. Ukoncujem...\n");
            // zatvorime pipe fds, ktore sme vytvorili pred volanim menu
            close(pipe_fd[0]);
            close(pipe_fd[1]);
            volba_pokracovat = 0;
            break;
        }

        // 3. Rozdelenie procesov
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            break;
        }

        if (pid == 0) {
            // --- PROCES SERVER (Dieťa) ---
            close(pipe_fd[0]); //server necita z pipe

            // Tu musí bežať SERVER a dostane zápisový koniec pipe
            spusti_server(shm, pipe_fd[1]);

            close(pipe_fd[1]);
            //odpojenie bloku od virtualnej adresnej mapy(nepotrebuje pristupovat ku zdielanej pamati)
            shmdt(shm);
            exit(0);
        } else {
            // --- PROCES KLIENT (Rodič) ---
            close(pipe_fd[1]); // Klient nezapisuje do pipe

            // OPRAVA: Tu musí bežať KLIENT a dostane čítací koniec pipe
            spusti_klienta(shm, pipe_fd[0]);

            close(pipe_fd[0]);
            // 4. Otázka na pokračovanie
            // Počkáme na smrť klienta, aby sa nám nepomiešali výpisy v konzole
            // --- PRIDAJTE TENTO BLOK ---
            printf("[MAIN] Posielam signal na ukoncenie servera (PID: %d)...\n", pid);
            // Po návrate klienta počkáme, aby server mohol korektne ukončiť výpočty.
            // Nechceme ho hneď zabíjať - najprv počkáme niekoľko sekúnd, potom
            // pošleme SIGTERM a nakoniec SIGKILL ako posledný prostriedok.
            printf("[MAIN] Cakam na ukoncenie servera (PID: %d)...\n", pid);
            int wait_seconds = 0;
            int status;
            while (wait_seconds < 5) {
                pid_t r = waitpid(pid, &status, WNOHANG);
                if (r == pid) {
                    // server uz skoncilo
                    break;
                }
                sleep(1);
                wait_seconds++;
            }
            if (wait_seconds >= 5) {
                // pokus o normalne ukoncenie
                printf("[MAIN] Server nereaguje, posielam SIGTERM (PID: %d)...\n", pid);
                kill(pid, SIGTERM);
                sleep(1);
                pid_t r2 = waitpid(pid, &status, WNOHANG);
                if (r2 != pid) {
                    printf("[MAIN] Server stale bezi, posielam SIGKILL (PID: %d)...\n", pid);
                    kill(pid, SIGKILL);
                    waitpid(pid, &status, 0);
                }
            } else {
                printf("[MAIN] Server ukoncil sa sam (PID: %d)\n", pid);
            }

            //kontrola aby zadal pouzivatel bud 0 alebo 1
            do {
                printf("\nChces spustit uplne novu simulaciu? (1 - ANO, 0 - KONIEC): ");
                fflush(stdout);

                // Kontrola, či bolo zadané číslo
                if (scanf(" %d", &volba_pokracovat) != 1) {
                    printf("Chyba: Musis zadat cislo (0 alebo 1)!\n");
                    while (getchar() != '\n');
                    volba_pokracovat = -1; // Nastavíme nevalidnú hodnotu, aby cyklus pokračoval
                    continue;
                }
                // Kontrola, či je číslo v povolenom rozsahu
                if (volba_pokracovat != 0 && volba_pokracovat != 1) {
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
