//
// Created by damko on 23/12/2025.
//

#ifndef SEMESTRALKA_CLIENT_LOGIC_H
#define SEMESTRALKA_CLIENT_LOGIC_H

#include "ipc_shm.h"

/**
 * Funkcia klienta, ktora caka na signal od servera a vykresluje mapu.
 */
void spusti_klienta(ZdielaneData_t* shm);

#endif //SEMESTRALKA_CLIENT_LOGIC_H