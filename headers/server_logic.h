//
// Created by damko on 16/12/2025.
//

#ifndef SEMESTRALKA_SERVER_LOGIC_H
#define SEMESTRALKA_SERVER_LOGIC_H

#include "common.h"
#include "ipc_shm.h"

void spusti_server(ZdielaneData_t* shm);
void uloz_vysledky_do_suboru(ZdielaneData_t* shm);
bool je_svet_validny(ZdielaneData_t* shm);
void generuj_svet_s_prekazkami(ZdielaneData_t* shm, int percento_prekazok);
int vyber_smeru(ZdielaneData_t* shm);
void simuluj_chodzu_z_policka(ZdielaneData_t* shm, int start_r, int start_s);

#endif