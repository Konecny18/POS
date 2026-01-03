cmake_minimum_required(VERSION 3.28)
project(Semestralka C)

set(CMAKE_C_STANDARD 11)

# Vyhľadanie knižnice pre vlákna (pthread)
find_package(Threads REQUIRED)

# Nastavenie cesty k hlavičkám
include_directories(${CMAKE_SOURCE_DIR}/headers)

# Zoznam všetkých zdrojových súborov, ktoré projekt potrebuje
set(ALL_SOURCES
        main.c
        server.c
        klient.c
        ipc_shm.c
        menu.c
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

#[[
cmake_minimum_required(VERSION 3.28.3)

project(Semestralka C)



set(CMAKE_C_STANDARD 11)



find_package(Threads REQUIRED)



# Include headers directory for all targets

set(HEADER_DIR ${CMAKE_SOURCE_DIR}/headers)

include_directories(${HEADER_DIR})



# Build a small static library for common IPC / SHM code so it can be reused by multiple targets

add_library(ipc_common STATIC ipc_shm.c)

target_include_directories(ipc_common PUBLIC ${HEADER_DIR})



# Server executable

add_executable(server server.c)

target_include_directories(server PRIVATE ${HEADER_DIR})

target_link_libraries(server PRIVATE ipc_common Threads::Threads)

if(UNIX)

    target_link_libraries(server PRIVATE rt)

endif()



# Client executable

add_executable(client klient.c)

target_include_directories(client PRIVATE ${HEADER_DIR})

target_link_libraries(client PRIVATE ipc_common Threads::Threads)

if(UNIX)

    target_link_libraries(client PRIVATE rt)

endif()



# Optional: original combined executable (keeps existing behavior if desired)

add_executable(semestralka main.c menu.c)

target_include_directories(semestralka PRIVATE ${HEADER_DIR})

target_link_libraries(semestralka PRIVATE ipc_common Threads::Threads)

if(UNIX)

    target_link_libraries(semestralka PRIVATE rt)

endif()



# Custom 'all' target that depends on server and client (and semestralka)

# NOTE: CMake already has a default 'all' target; defining it here ensures

# a convenient explicit target is present for the assignment requirements.

add_custom_target(all DEPENDS server client semestralka)



# Custom 'clean' target that forwards to the build system's clean

add_custom_target(clean

        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target clean

        COMMENT "Cleaning build directory via CMake"

)]]