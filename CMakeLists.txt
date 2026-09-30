cmake_minimum_required(VERSION 4.3)
project(caos_vehicle_charging C)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_STANDARD 11)

add_executable(caos_vehicle_charging
        app/main.c
        src/vehicle.c
        src/charger.c
        src/station.c
        src/config.c
        src/config_file.c
        src/sim_context.c
)

target_include_directories(caos_vehicle_charging
        PUBLIC include
)
