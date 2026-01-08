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
#include <sys/socket.h>

#include "../headers/ipc_shm.h"
#include "../headers/server_logic.h"
#include "../headers/client_logic.h"
#include "../headers/client_menu.h"

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
    key_t key = 1234; // Unikátny kľúč pre zdieľanú pamäť
    int pipe_fd[2]; // Deskriptory pre anonymnú rúru (0: čítanie, 1: zápis)

    // --- 1. PRÍPRAVA INFRAŠTRUKTÚRY ---
    // Vytvorím segment zdieľanej pamäte a inicializujeme v ňom semafory.
    ZdielaneData_t* shm = shm_create_and_attach(key);
    if (shm == NULL) {
        fprintf(stderr, "Nepodarilo sa vytvorit SHM.\n");
        return -1;
    }

    int volba_pokracovat = 1;
    while (volba_pokracovat) {

        // Vytvorím rúru (pipe) pre prenos textových logov zo servera ku klientovi.
        if (pipe(pipe_fd) == -1) {
            perror("pipe");
            return -1;
        }

        // --- 2. INTERAKCIA S POUŽÍVATEĽOM ---
        // Volanie menu nastaví parametre v 'shm' a prepne stav na SIM_INIT.
        int volba = zobraz_pociatocne_menu(shm);

        // Špeciálny prípad: Klient sa chce len pripojiť k už bežiacej simulácii.
        if (volba == 3) {
            printf("[MAIN] Pripajam sa k existujucej simulacii...\n");
            spusti_klienta(shm, -1, -1);
            continue;
        }

        // Ak v menu zvolil 0 (Koniec), upracem rúry a ukončím aplikáciu.
        if (shm->stav == SIM_EXIT) {
            printf("[MAIN] Pouzivatel zvolil ukoncenie aplikacie v menu. Ukoncujem...\n");
            // zatvorime pipe fds, ktore sme vytvorili pred volanim menu
            close(pipe_fd[0]);
            close(pipe_fd[1]);
            volba_pokracovat = 0;
            break;
        }

        // Vytvorenie obojsmerného socketu pre ovládanie servera (napr. kláves 'v' alebo 'q').
        int sv[2];
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == -1) {
            perror("socketpair");
            return -1;
        }

        // --- 3. ROZDELENIE NA SERVER A KLIENTA (FORK) ---
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            break;
        }

        if (pid == 0) {
            // --- PROCES SERVER (Dieťa) ---
            close(sv[0]);// server zatvori klientsky socket
            close(pipe_fd[0]); //server necita z pipe len pise logy


            // Server beží tu: zapisuje do pipe_fd[1] a počúva príkazy na sv[1]
            spusti_server(shm, pipe_fd[1], sv[1]);

            // Po skončení servera upracem deskriptory a odpojíme sa od SHM
            close(pipe_fd[1]);
            close(sv[1]);
            //odpojenie bloku od virtualnej adresnej mapy(nepotrebuje pristupovat ku zdielanej pamati)
            shmdt(shm);
            exit(0);
        } else {
            // --- PROCES KLIENT (Rodič) ---
            close(sv[1]);   //klient zatvori serverovy socket
            close(pipe_fd[1]); // Klient nezapisuje do pipe, cita z nej logy

            // Klient beží tu: číta z pipe_fd[0] a posiela príkazy cez sv[0]
            spusti_klienta(shm, pipe_fd[0], sv[0]);

            close(sv[0]);
            close(pipe_fd[0]);

            // --- 4. MANAŽMENT UKONČENIA SERVERA ---
            // Po zatvorení okna klienta musím dohliadnuť na korektné ukončenie servera.
            printf("[MAIN] Cakam na ukoncenie servera (PID: %d)...\n", pid);
            int wait_seconds = 0;
            int status;

            // Dám serveru 5 sekúnd, aby sa sám vypol (napr. uložil súbory)
            while (wait_seconds < 5) {
                pid_t r = waitpid(pid, &status, WNOHANG);
                if (r == pid) {
                    // Server úspešne dobehol
                    break;
                }
                sleep(1);
                wait_seconds++;
            }
            // Ak sa server nevypol sám, musím zasiahnuť (SIGTERM -> SIGKILL)
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

            // --- 5. REPLIKÁCIA ALEBO KONIEC ---
            // Kontrola vstupu, či chce používateľ začať znova od Menu.
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

    // --- 6. FINÁLNE UPRATOVANIE CELÉHO SYSTÉMU ---
    printf("\n[MAIN] Uvolnujem prostriedky a koncim aplikaciu...\n");

    // Zničenie semaforov a úplné odstránenie segmentu SHM z OS
    shm_cleanup_semaphores(shm);
    shm_detach_and_destroy(shm, key);

    return 0;
}
