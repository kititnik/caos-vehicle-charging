#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "config_file.h"
#include "sim_context.h"
#include "stats.h"

static const char* config_error_message(ErrorCode error);
static ErrorCode subscribe_all(SimContext* sim, Config* config, Station* station);
static void on_interrupt(int signum);
static const char* end_reason(const SimContext* sim);

static volatile sig_atomic_t interrupted = 0;

int main(int argc, char** argv) {
    if (argc != 3 || strcmp(argv[1], "--config") != 0) {
        fprintf(stderr, "Usage: %s --config <file>\n", argv[0]);
        return 1;
    }
    const char* config_path = argv[2];

    Config config;
    int error_line;
    ErrorCode error = config_file_load(&config, config_path, &error_line);
    if (error != ERR_OK) {
        if (error_line > 0) {
            fprintf(stderr, "%s:%d: %s\n", config_path, error_line, config_error_message(error));
        } else {
            fprintf(stderr, "%s: %s\n", config_path, config_error_message(error));
        }
        return 1;
    }

    SimContext sim;
    Logger logger;
    if (logger_init(&logger, &sim.now, config.log_path) != ERR_OK) {
        fprintf(stderr, "%s: cannot open log file\n", config.log_path);
        config_destroy(&config);
        return 1;
    }

    Station station;
    if (station_init(&station, &config, &logger) != ERR_OK) {
        fprintf(stderr, "cannot initialize station\n");
        logger_close(&logger);
        config_destroy(&config);
        return 1;
    }

    if (sim_init(&sim, &config, &station, &logger) != ERR_OK) {
        fprintf(stderr, "cannot initialize simulation\n");
        station_destroy(&station);
        logger_close(&logger);
        config_destroy(&config);
        return 1;
    }
    if (subscribe_all(&sim, &config, &station) != ERR_OK) {
        fprintf(stderr, "cannot subscribe participants\n");
        sim_destroy(&sim);
        station_destroy(&station);
        logger_close(&logger);
        config_destroy(&config);
        return 1;
    }
    if (signal(SIGINT, on_interrupt) == SIG_ERR) {
        fprintf(stderr, "cannot install interrupt handler\n");
        sim_destroy(&sim);
        station_destroy(&station);
        logger_close(&logger);
        config_destroy(&config);
        return 1;
    }

    while (!interrupted && !sim_is_finished(&sim)) {
        sim_tick(&sim);
        usleep(sim.display_delay_ms * 1000);
    }
    if (interrupted) {
        logger_write(&logger, "моделирование прервано пользователем");
    }
    stats_print(&sim, end_reason(&sim));

    sim_destroy(&sim);
    station_destroy(&station);
    logger_close(&logger);
    config_destroy(&config);
    return interrupted ? 128 + SIGINT : 0;
}

static ErrorCode subscribe_all(SimContext* sim, Config* config, Station* station) {
    ErrorCode error = ERR_OK;
    for (int i = 0; i < config->cars_count && error == ERR_OK; i++) {
        error = sim_subscribe(sim, &config->cars[i], vehicle_on_tick, ORDER_VEHICLE);
    }
    if (error == ERR_OK) {
        error = sim_subscribe(sim, station, station_on_tick, ORDER_STATION);
    }
    for (int i = 0; i < config->chargers_count && error == ERR_OK; i++) {
        error = sim_subscribe(sim, &config->chargers[i], charger_on_tick, ORDER_CHARGER);
    }
    return error;
}

static const char* end_reason(const SimContext* sim) {
    if (interrupted) {
        return "прервано пользователем";
    }
    if (sim->mode == MODE_LIMITED && sim->now >= sim->period_time) {
        return "окончание периода";
    }
    return "все машины обслужены";
}

static void on_interrupt(int signum) {
    interrupted = 1;
}

static const char* config_error_message(ErrorCode error) {
    switch (error) {
        case ERR_FILE_OPEN:
            return "cannot open file";
        case ERR_FILE_READ:
            return "cannot read file";
        case ERR_OUT_OF_MEMORY:
            return "out of memory";
        case ERR_CONFIG_SYNTAX:
            return "syntax error";
        case ERR_CONFIG_VALUE:
            return "invalid value";
        case ERR_CONFIG_MISSING:
            return "missing required parameters";
        default:
            return "unknown error";
    }
}
