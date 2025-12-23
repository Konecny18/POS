cmake_minimum_required(VERSION 3.28.3)
project(Semestralka C)

set(CMAKE_C_STANDARD 11)

# Pridanie všetkých potrebných súborov (odstránil som duplicity)
add_executable(Semestralka
        main.c
        server.c
        klient.c
        ipc_shm.c
        headers/server_logic.h
        headers/client_logic.h
        headers/common.h
        headers/ipc_shm.h
        headers/client_menu.h
        headers/client_render.h
)

# KĽÚČOVÉ PRE LINUX: Prepojenie knižníc pre vlákna (pthread) a zdieľanú pamäť (rt)
target_link_libraries(Semestralka pthread rt)