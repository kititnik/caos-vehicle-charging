cmake_minimum_required(VERSION 3.22)
project(caos_vehicle_charging C)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_STANDARD 11)

add_library(caos_common STATIC
        src/vehicle.c
        src/charger.c
        src/station.c
        src/config.c
        src/config_file.c
        src/sim_context.c
        src/logger.c
        src/stats.c
        src/net.c
        src/remote.c
)
target_include_directories(caos_common PUBLIC include)

add_executable(caos_vehicle_charging app/main.c)
target_link_libraries(caos_vehicle_charging PRIVATE caos_common)

add_executable(cvc_server app/server.c)
target_link_libraries(cvc_server PRIVATE caos_common)

add_executable(cvc_client app/client.c)
target_link_libraries(cvc_client PRIVATE caos_common)
