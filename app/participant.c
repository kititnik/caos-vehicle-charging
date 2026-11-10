#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "shared_state.h"
#include "sim_context.h"

#define RETRY_DELAY_US 100000

// --role values
static const char* const ROLE_NAMES[ORDER_COUNT] = {"vehicle", "station", "charger"};

typedef struct Participant {
    Sync ready;
    Shared shared;
    Logger logger;
    Station station;
    SimContext sim;
} Participant;

typedef enum {
    OPENED_NOTHING,
    OPENED_READY,
    OPENED_SHARED,
    OPENED_LOGGER,
    OPENED_STATION,
    OPENED_SIM
} Opened;

static ErrorCode join_run(Participant* participant, const char* run, TickOrder role, int index);
static void leave_run(Participant* participant, Opened opened);
static ErrorCode subscribe(Participant* participant, TickOrder role, int index);
static void on_interrupt(int signum);

static volatile sig_atomic_t interrupted = 0;
// stop flag in shared memory
static int* volatile stop = NULL;

int main(int argc, char** argv) {
    const char* run = "main";
    TickOrder role = ORDER_COUNT;
    int index = -1;

    int valid = argc % 2 == 1;
    for (int i = 1; valid && i < argc; i += 2) {
        if (strcmp(argv[i], "--run") == 0) {
            run = argv[i + 1];
        } else if (strcmp(argv[i], "--role") == 0) {
            for (int k = 0; k < ORDER_COUNT; k++) {
                if (strcmp(argv[i + 1], ROLE_NAMES[k]) == 0) {
                    role = k;
                }
            }
        } else if (strcmp(argv[i], "--index") == 0) {
            char* end;
            index = (int)strtol(argv[i + 1], &end, 10);
            valid = end != argv[i + 1] && *end == '\0';
        } else {
            valid = 0;
        }
    }
    if (role == ORDER_STATION) {
        index = 0;
    }
    if (!valid || role == ORDER_COUNT || index < 0) {
        fprintf(stderr, "Usage: %s --role vehicle|station|charger [--index n] [--run <id>]\n", argv[0]);
        return 1;
    }

    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = on_interrupt;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGINT, &action, NULL) != 0) {
        fprintf(stderr, "cannot install interrupt handler\n");
        return 1;
    }

    Participant participant;
    ErrorCode error;
    while ((error = join_run(&participant, run, role, index)) == ERR_NOT_FOUND && !interrupted) {
        usleep(RETRY_DELAY_US);
    }
    if (error != ERR_OK) {
        if (!interrupted) {
            fprintf(stderr, "cannot join run %s\n", run);
        }
        return interrupted ? 128 + SIGINT : 1;
    }

    stop = &participant.shared.state->stop;
    if (interrupted) {
        *stop = 1;
    }
    sync_post(&participant.ready);
    sim_run_subscriber(&participant.sim, participant.sim.subscribers[0]);

    stop = NULL;
    leave_run(&participant, OPENED_SIM);
    return interrupted ? 128 + SIGINT : 0;
}

static ErrorCode join_run(Participant* participant, const char* run, TickOrder role, int index) {
    Opened opened = OPENED_NOTHING;
    ErrorCode error = sync_open(&participant->ready, run, "ready", 0);
    if (error == ERR_OK) {
        opened = OPENED_READY;
        error = shared_state_open(&participant->shared, run);
    }
    if (error == ERR_OK) {
        opened = OPENED_SHARED;
        SharedState* state = participant->shared.state;
        const char* log_path = state->log_path[0] != '\0' ? state->log_path : NULL;
        error = logger_init(&participant->logger, &state->now, log_path, run, 0);
    }
    if (error == ERR_OK) {
        opened = OPENED_LOGGER;
        Shared* shared = &participant->shared;
        error = station_init(&participant->station, shared->state, shared->cars, shared->chargers, shared->queue,
                             &participant->logger, run, 0);
    }
    if (error == ERR_OK) {
        opened = OPENED_STATION;
        error = sim_init(&participant->sim, participant->shared.state, participant->shared.cars,
                         &participant->station, &participant->logger, run, 0);
    }
    if (error == ERR_OK) {
        opened = OPENED_SIM;
        error = subscribe(participant, role, index);
    }
    if (error != ERR_OK) {
        leave_run(participant, opened);
    }
    return error;
}

static void leave_run(Participant* participant, Opened opened) {
    if (opened >= OPENED_SIM) {
        sim_destroy(&participant->sim);
    }
    if (opened >= OPENED_STATION) {
        station_destroy(&participant->station);
    }
    if (opened >= OPENED_LOGGER) {
        logger_close(&participant->logger);
    }
    if (opened >= OPENED_SHARED) {
        shared_state_close(&participant->shared);
    }
    if (opened >= OPENED_READY) {
        sync_close(&participant->ready);
    }
}

static ErrorCode subscribe(Participant* participant, TickOrder role, int index) {
    Shared* shared = &participant->shared;
    switch (role) {
        case ORDER_VEHICLE:
            if (index >= shared->state->cars_count) {
                return ERR_INVALID_ARGUMENT;
            }
            return sim_subscribe(&participant->sim, &shared->cars[index], vehicle_on_tick, role, index);
        case ORDER_STATION:
            return sim_subscribe(&participant->sim, &participant->station, station_on_tick, role, 0);
        default:
            if (index >= shared->state->chargers_count) {
                return ERR_INVALID_ARGUMENT;
            }
            return sim_subscribe(&participant->sim, &shared->chargers[index], charger_on_tick, role, index);
    }
}

static void on_interrupt(int signum) {
    interrupted = 1;
    if (stop != NULL) {
        *stop = 1;
    }
}
