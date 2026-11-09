cmake_minimum_required(VERSION 3.22)
project(caos_vehicle_charging C)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_STANDARD 11)

# everything except the semaphore implementation
add_library(caos_common STATIC
        src/vehicle.c
        src/charger.c
        src/station.c
        src/config.c
        src/config_file.c
        src/sim_context.c
        src/logger.c
        src/stats.c
        src/sync.c
        src/shared_memory.c
        src/shared_state.c
        src/channel.c
        src/world.c
)
target_include_directories(caos_common PUBLIC include)

find_package(Threads REQUIRED)
target_link_libraries(caos_common PUBLIC Threads::Threads)

# program 1: named semaphores
add_executable(caos_named app/main.c src/sync_named.c)
target_link_libraries(caos_named PRIVATE caos_common)

# program 2: unnamed semaphores in shared memory
add_executable(caos_unnamed app/main.c src/sync_unnamed.c)
target_link_libraries(caos_unnamed PRIVATE caos_common)

# program 3: independent processes, role is chosen by --role
add_executable(cvc_participant app/participant.c src/sync_named.c)
target_link_libraries(cvc_participant PRIVATE caos_common)

add_executable(cvc_coordinator app/coordinator.c src/sync_named.c)
target_link_libraries(cvc_coordinator PRIVATE caos_common)

# program 4: independent processes, data goes through named pipes
add_executable(cvc_fifo_coordinator app/fifo_coordinator.c src/sync_named.c)
target_link_libraries(cvc_fifo_coordinator PRIVATE caos_common)

add_executable(cvc_fifo_participant app/fifo_participant.c src/sync_named.c)
target_link_libraries(cvc_fifo_participant PRIVATE caos_common)
