cmake_minimum_required(VERSION 3.28.3)
project(Semestralka C)

set(CMAKE_C_STANDARD 11)

add_executable(Semestralka main.c
        server.c
        klient.c
        headers/server_logic.h
        headers/client_menu.h
        headers/client_render.h
        headers/common.h
        headers/ipc_shm.h
        ipc_shm.c
        headers/ipc_shm.h
        ipc_shm.c
        ipc_shm.c
        ipc_shm.c
)
