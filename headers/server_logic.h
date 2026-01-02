//
// Created by damko on 16/12/2025.
//

#ifndef SEMESTRALKA_SERVER_LOGIC_H
#define SEMESTRALKA_SERVER_LOGIC_H

/**
 * @file server_logic.h
 * @brief Deklarácie funkcií riadiacich logiku servera simulácie chôdze.
 *
 * Tento header obsahuje prototypy funkcií implementovaných v `server.c`.
 * Funkcie zahŕňajú inicializáciu sveta, generovanie prekážok, samotnú
 * simuláciu jedného chodca, vykonanie sumárnej simulácie pre všetky políčka
 * a ukladanie/načítanie konfigurácie a výsledkov zo súboru.
 */

#include "common.h"
#include "ipc_shm.h"

/**
 * @brief Spustí hlavnú riadiacu slučku servera (čakanie na klienta, spustenie simulácie).
 *
 * @param shm Ukazovateľ na zdieľanú pamäť obsahujúcu konfiguráciu a výsledky.
 * @param pipe_write_fd File descriptor pre zápis do pipe smerom ku klientovi (log/notify).
 * @param socket_fd File descriptor TCP/Unix socketu použitý pre sieťovú komunikáciu (ak je používaný), alebo -1.
 */
void spusti_server(ZdielaneData_t* shm, int pipe_write_fd, int socket_fd);

/**
 * @brief Uloží výsledky simulácie a konfiguráciu sveta do súboru.
 *
 * Ukladá rozmery, počet replikácií, maximálny počet krokov, počet prekážok,
 * pravdepodobnosti pohybov, mapu sveta a agregované výsledky pre každé políčko.
 * @param shm Ukazovateľ na zdieľanú štruktúru.
 */
void uloz_vysledky_do_suboru(ZdielaneData_t* shm);

/**
 * @brief Načíta konfiguráciu a mapu sveta zo súboru do zdieľanej pamäte.
 *
 * Očakáva formát uložený funkciou `uloz_vysledky_do_suboru`.
 * @param shm Ukazovateľ na zdieľanú štruktúru, kde sa načítané hodnoty uložia.
 * @return true ak bolo načítanie úspešné, inak false.
 */
bool nacitaj_konfig_zo_suboru(ZdielaneData_t* shm);

/**
 * @brief Overí, či je svet priechodný (všetky neprekážkové políčka dosiahnuteľné z [0,0]).
 *
 * Používa BFS na toroidnej mriežke.
 * @param shm Ukazovateľ na zdieľanú štruktúru obsahujúcu mapu a rozmery.
 * @return true ak sú všetky neprekážkové políčka dosiahnuteľné, inak false.
 */
bool je_svet_validny(ZdielaneData_t* shm);

/**
 * @brief Vygeneruje náhodný svet s prekážkami podľa percenta.
 *
 * @param shm Ukazovateľ na zdieľanú štruktúru.
 * @param percento_prekazok Celé percento (0-100) šance, že políčko bude prekážka.
 */
void generuj_svet_s_prekazkami(ZdielaneData_t* shm, int percento_prekazok);

/**
 * @brief Vyberie smer pohybu na základe nastavených pravdepodobností.
 *
 * Indexy: 0 = HORE, 1 = DOLE, 2 = VLAVO, 3 = VPRAVO.
 * @param shm Ukazovateľ na zdieľanú štruktúru obsahujúcu pole pravdepodobností.
 * @return Číslo v rozsahu 0..3 reprezentujúce smer.
 */
int vyber_smeru(ZdielaneData_t* shm);

/**
 * @brief Simuluje jedného chodca z daného štartovacieho políčka.
 *
 * Po skončení replikácie aktualizuje štatistiky (priemerné kroky a počet úspechov)
 * pre štartovacie políčko.
 * @param shm Ukazovateľ na zdieľanú štruktúru.
 * @param start_r Počiatočný riadok chodca.
 * @param start_s Počiatočný stĺpec chodca.
 */
void simuluj_chodzu_z_policka(ZdielaneData_t* shm, int start_r, int start_s);

/**
 * @brief Inicializuje herný svet pre server (načíta zo súboru alebo vygeneruje nový).
 *
 * @param shm Ukazovateľ na zdieľanú štruktúru.
 * @return true ak bol svet úspešne inicializovaný, false pri chybe alebo stop požiadavke.
 */
bool inicializuj_svet_servera(ZdielaneData_t* shm);

/**
 * @brief Vykoná kompletnú sumárnu simuláciu pre všetky políčka sveta.
 *
 * Pre každú replikáciu prejde všetky políčka a spustí z neho simuláciu chôdze ak nie je prekážka.
 *
 * @param shm Ukazovateľ na zdieľanú štruktúru.
 * @param pipe_write_fd File descriptor pre zápis log/štavových správ klientovi cez pipe.
 * @param socket_fd File descriptor socketu pre prípadné odosielanie výsledkov cez sieť (alebo -1 ak nepoužité).
 * @param p_rezim_logovania Ukazovateľ na int, ktorý určuje režim logovania (0 = žiadny, 1 = stručný, 2 = detailný); môže byť NULL.
 */
void vykonaj_sumarnu_simulaciu(ZdielaneData_t* shm, int pipe_write_fd, int socket_fd, int* p_rezim_logovania);

/**
 * @brief Posiela textové log správy cez pipe na klienta.
 *
 * Použité na zasielanie krátkych notifikácií alebo priebežných informácií server -> klient.
 * @param pipe_write_fd File descriptor pipe pre zápis.
 * @param sprava C-string správa (null-terminated), bezpečné pre lokálny buffer.
 */
void posli_log(int pipe_write_fd, const char* sprava);

#endif // SEMESTRALKA_SERVER_LOGIC_H
