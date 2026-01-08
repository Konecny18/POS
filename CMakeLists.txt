cmake_minimum_required(VERSION 3.28)
project(Semestralka C)

set(CMAKE_C_STANDARD 11)

# Vyhľadanie knižnice pre vlákna (pthread)
find_package(Threads REQUIRED)

# Nastavenie cesty k hlavičkám
include_directories(${CMAKE_SOURCE_DIR}/headers)

# Zoznam všetkých zdrojových súborov, ktoré projekt potrebuje
set(ALL_SOURCES
        logika_aplikacie/main.c
        logika_aplikacie/server.c
        logika_aplikacie/klient.c
        logika_aplikacie/ipc_shm.c
        logika_aplikacie/menu.c
)

# --- CIEL 1: server ---
add_executable(server ${ALL_SOURCES})
target_link_libraries(server PRIVATE Threads::Threads rt)

# --- CIEL 2: client ---
add_executable(client ${ALL_SOURCES})
target_link_libraries(client PRIVATE Threads::Threads rt)

# --- CIEL 3: all ---
# CMake automaticky vytvorí cieľ 'all', ktorý zostaví server aj client.

# --- CIEL 4: clean ---
# CMake automaticky vytvorí cieľ 'clean'.
