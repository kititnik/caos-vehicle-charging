#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "sim_context.h"
#include "world.h"

#define RETRY_DELAY_US 100000

typedef struct Participant {
    Sync ready;
    Channel down;
    Channel up;
    World world;
    Logger logger;
    Station station;
    SimContext sim;
} Participant;

typedef enum {
    OPENED_NOTHING,
    OPENED_READY,
    OPENED_DOWN,
    OPENED_UP,
    OPENED_LOGGER,
    OPENED_STATION,
    OPENED_SIM
} Opened;

static ErrorCode join_run(Participant* participant, const char* run, TickOrder role, int index);
static void leave_run(Participant* participant, Opened opened);
static ErrorCode subscribe(Participant* participant, TickOrder role, int index);
static void on_interrupt(int signum);

static volatile sig_atomic_t interrupted = 0;

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
                if (strcmp(argv[i + 1], ORDER_NAMES[k]) == 0) {
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
    memset(&participant, 0, sizeof(participant));
    ErrorCode error;
    while ((error = sync_open(&participant.ready, run, "ready", 0)) == ERR_NOT_FOUND && !interrupted) {
        usleep(RETRY_DELAY_US);
    }
    if (error == ERR_OK) {
        sync_post(&participant.ready);
        error = join_run(&participant, run, role, index);
    }
    if (error != ERR_OK) {
        if (!interrupted) {
            fprintf(stderr, "cannot join run %s\n", run);
        }
        return interrupted ? 128 + SIGINT : 1;
    }

    TickSubscriber* subscriber = participant.sim.subscribers[0];
    while (world_receive(&participant.down, &participant.world) == ERR_OK) {
        sync_lock(&subscriber->start);
        subscriber->handler(subscriber->self, &participant.sim);
        if (interrupted) {
            participant.world.state->stop = 1;
        }
        if (world_send(&participant.up, &participant.world) != ERR_OK) {
            break;
        }
        sync_post(&participant.sim.phase_done[subscriber->order]);
    }

    leave_run(&participant, OPENED_SIM);
    return interrupted ? 128 + SIGINT : 0;
}

static ErrorCode join_run(Participant* participant, const char* run, TickOrder role, int index) {
    Opened opened = OPENED_READY;
    ErrorCode error = channel_open(&participant->down, run, ORDER_NAMES[role], index, "down", O_RDONLY);
    if (error == ERR_OK) {
        opened = OPENED_DOWN;
        error = channel_open(&participant->up, run, ORDER_NAMES[role], index, "up", O_WRONLY);
    }
    if (error == ERR_OK) {
        opened = OPENED_UP;
        error = world_receive(&participant->down, &participant->world);
    }
    if (error == ERR_OK) {
        SharedState* state = participant->world.state;
        const char* log_path = state->log_path[0] != '\0' ? state->log_path : NULL;
        error = logger_init(&participant->logger, &state->now, log_path, run, 0);
    }
    if (error == ERR_OK) {
        opened = OPENED_LOGGER;
        World* world = &participant->world;
        error = station_init(&participant->station, world->state, world->cars, world->chargers, world->queue,
                             &participant->logger, run, 0);
    }
    if (error == ERR_OK) {
        opened = OPENED_STATION;
        error = sim_init(&participant->sim, participant->world.state, participant->world.cars,
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
    if (opened >= OPENED_UP) {
        channel_close(&participant->up);
    }
    if (opened >= OPENED_DOWN) {
        channel_close(&participant->down);
    }
    if (opened >= OPENED_READY) {
        sync_close(&participant->ready);
    }
    world_destroy(&participant->world);
}

static ErrorCode subscribe(Participant* participant, TickOrder role, int index) {
    World* world = &participant->world;
    switch (role) {
        case ORDER_VEHICLE:
            if (index >= world->state->cars_count) {
                return ERR_INVALID_ARGUMENT;
            }
            return sim_subscribe(&participant->sim, &world->cars[index], vehicle_on_tick, role, index);
        case ORDER_STATION:
            return sim_subscribe(&participant->sim, &participant->station, station_on_tick, role, 0);
        default:
            if (index >= world->state->chargers_count) {
                return ERR_INVALID_ARGUMENT;
            }
            return sim_subscribe(&participant->sim, &world->chargers[index], charger_on_tick, role, index);
    }
}

static void on_interrupt(int signum) {
    interrupted = 1;
}
