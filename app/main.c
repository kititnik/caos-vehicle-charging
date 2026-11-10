#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "config_file.h"
#include "shared_state.h"
#include "sim_context.h"
#include "stats.h"

static const char* config_error_message(ErrorCode error);
static ErrorCode subscribe_all(SimContext* sim, Shared* shared, Station* station);
static void on_interrupt(int signum);
static const char* end_reason(const SimContext* sim);

// set by ctrl+c handler, checked in main loop
static volatile sig_atomic_t interrupted = 0;

// load config, put it into shared memory, fork participants, run until done or ctrl+c, print stats
int main(int argc, char** argv) {
    if (argc != 3 || strcmp(argv[1], "--config") != 0) {
        fprintf(stderr, "Usage: %s --config <file>\n", argv[0]);
        return 1;
    }
    const char* config_path = argv[2];

    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = on_interrupt;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGINT, &action, NULL) != 0) {
        fprintf(stderr, "cannot install interrupt handler\n");
        return 1;
    }

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

    char run[16];
    snprintf(run, sizeof(run), "%d", (int)getpid());

    Shared shared;
    if (shared_state_create(&shared, &config, run) != ERR_OK) {
        fprintf(stderr, "cannot create shared memory\n");
        config_destroy(&config);
        return 1;
    }

    Logger logger;
    error = logger_init(&logger, &shared.state->now, config.log_path, run, 1);
    if (error != ERR_OK) {
        if (error == ERR_FILE_OPEN) {
            fprintf(stderr, "%s: cannot open log file\n", config.log_path);
        } else {
            fprintf(stderr, "cannot initialize logger\n");
        }
        shared_state_destroy(&shared);
        config_destroy(&config);
        return 1;
    }
    config_destroy(&config);

    Station station;
    if (station_init(&station, shared.state, shared.cars, shared.chargers, shared.queue, &logger, run, 1) != ERR_OK) {
        fprintf(stderr, "cannot initialize station\n");
        logger_close(&logger);
        shared_state_destroy(&shared);
        return 1;
    }

    SimContext sim;
    if (sim_init(&sim, shared.state, shared.cars, &station, &logger, run, 1) != ERR_OK) {
        fprintf(stderr, "cannot initialize simulation\n");
        station_destroy(&station);
        logger_close(&logger);
        shared_state_destroy(&shared);
        return 1;
    }
    if (subscribe_all(&sim, &shared, &station) != ERR_OK) {
        fprintf(stderr, "cannot subscribe participants\n");
        sim_destroy(&sim);
        station_destroy(&station);
        logger_close(&logger);
        shared_state_destroy(&shared);
        return 1;
    }
    if (sim_start(&sim) != ERR_OK) {
        fprintf(stderr, "cannot start participant processes\n");
        sim_destroy(&sim);
        station_destroy(&station);
        logger_close(&logger);
        shared_state_destroy(&shared);
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
    shared_state_destroy(&shared);
    // 130 on ctrl+c
    return interrupted ? 128 + SIGINT : 0;
}

static ErrorCode subscribe_all(SimContext* sim, Shared* shared, Station* station) {
    ErrorCode error = ERR_OK;
    for (int i = 0; i < shared->state->cars_count && error == ERR_OK; i++) {
        error = sim_subscribe(sim, &shared->cars[i], vehicle_on_tick, ORDER_VEHICLE, i);
    }
    if (error == ERR_OK) {
        error = sim_subscribe(sim, station, station_on_tick, ORDER_STATION, 0);
    }
    for (int i = 0; i < shared->state->chargers_count && error == ERR_OK; i++) {
        error = sim_subscribe(sim, &shared->chargers[i], charger_on_tick, ORDER_CHARGER, i);
    }
    return error;
}

static const char* end_reason(const SimContext* sim) {
    if (interrupted) {
        return "прервано пользователем";
    }
    if (sim->mode == MODE_LIMITED && sim->state->now >= sim->period_time) {
        return "окончание периода";
    }
    return "все машины обслужены";
}

// just set a flag, nothing else is safe in a signal handler
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
