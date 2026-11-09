#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config_file.h"
#include "sim_context.h"
#include "stats.h"
#include "world.h"

static const char* config_error_message(ErrorCode error);
static ErrorCode subscribe_all(SimContext* sim, World* world, Station* station);
static ErrorCode create_channels(SimContext* sim, const char* run, Channel* down, Channel* up);
static void destroy_channels(SimContext* sim, Channel* down, Channel* up);
static ErrorCode open_channels(SimContext* sim, const char* run, Channel* down, Channel* up, World* world);
static ErrorCode exchange_tick(SimContext* sim, World* world, World* answer, Channel* down, Channel* up);
static void apply(SimContext* sim, World* world, const World* answer, const TickSubscriber* subscriber);
static void on_interrupt(int signum);
static const char* end_reason(const SimContext* sim);

static volatile sig_atomic_t interrupted = 0;

int main(int argc, char** argv) {
    const char* config_path = NULL;
    const char* run = "main";

    int valid = argc % 2 == 1;
    for (int i = 1; valid && i < argc; i += 2) {
        if (strcmp(argv[i], "--config") == 0) {
            config_path = argv[i + 1];
        } else if (strcmp(argv[i], "--run") == 0) {
            run = argv[i + 1];
        } else {
            valid = 0;
        }
    }
    if (!valid || config_path == NULL) {
        fprintf(stderr, "Usage: %s --config <file> [--run <id>]\n", argv[0]);
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
    // a dead participant gives EPIPE from write instead of killing us
    signal(SIGPIPE, SIG_IGN);

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

    World world;
    if (world_create(&world, &config) != ERR_OK) {
        fprintf(stderr, "out of memory\n");
        config_destroy(&config);
        return 1;
    }

    Logger logger;
    error = logger_init(&logger, &world.state->now, config.log_path, run, 1);
    if (error != ERR_OK) {
        if (error == ERR_FILE_OPEN) {
            fprintf(stderr, "%s: cannot open log file\n", config.log_path);
        } else {
            fprintf(stderr, "cannot initialize logger\n");
        }
        world_destroy(&world);
        config_destroy(&config);
        return 1;
    }
    config_destroy(&config);

    Station station;
    if (station_init(&station, world.state, world.cars, world.chargers, world.queue, &logger, run, 1) != ERR_OK) {
        fprintf(stderr, "cannot initialize station\n");
        logger_close(&logger);
        world_destroy(&world);
        return 1;
    }

    SimContext sim;
    if (sim_init(&sim, world.state, world.cars, &station, &logger, run, 1) != ERR_OK) {
        fprintf(stderr, "cannot initialize simulation\n");
        station_destroy(&station);
        logger_close(&logger);
        world_destroy(&world);
        return 1;
    }
    if (subscribe_all(&sim, &world, &station) != ERR_OK) {
        fprintf(stderr, "cannot subscribe participants\n");
        sim_destroy(&sim);
        station_destroy(&station);
        logger_close(&logger);
        world_destroy(&world);
        return 1;
    }

    Channel* down = calloc((size_t)sim.subscribers_count, sizeof(Channel));
    Channel* up = calloc((size_t)sim.subscribers_count, sizeof(Channel));
    if (down == NULL || up == NULL || create_channels(&sim, run, down, up) != ERR_OK) {
        fprintf(stderr, "cannot create pipes\n");
        if (down != NULL && up != NULL) {
            destroy_channels(&sim, down, up);
        }
        free(down);
        free(up);
        sim_destroy(&sim);
        station_destroy(&station);
        logger_close(&logger);
        world_destroy(&world);
        return 1;
    }

    Sync ready;
    if (sync_create(&ready, run, "ready", 0, 0) != ERR_OK) {
        fprintf(stderr, "cannot create ready semaphore\n");
        destroy_channels(&sim, down, up);
        free(down);
        free(up);
        sim_destroy(&sim);
        station_destroy(&station);
        logger_close(&logger);
        world_destroy(&world);
        return 1;
    }

    logger_write(&logger, "ожидание участников (run %s): %d", run, sim.subscribers_count);
    int joined = 0;
    while (joined < sim.subscribers_count && !interrupted) {
        if (sync_wait(&ready) == ERR_OK) {
            joined++;
        }
    }
    error = joined == sim.subscribers_count ? open_channels(&sim, run, down, up, &world) : ERR_INTERRUPTED;
    if (error == ERR_OK) {
        logger_write(&logger, "все участники подключились");
    }

    World answer;
    memset(&answer, 0, sizeof(answer));
    while (error == ERR_OK && !interrupted && !world.state->stop && !sim_is_finished(&sim)) {
        error = exchange_tick(&sim, &world, &answer, down, up);
        usleep(sim.display_delay_ms * 1000);
    }
    if (error == ERR_CHANNEL) {
        logger_write(&logger, "связь с участником потеряна");
        world.state->stop = 1;
    }
    int stopped = interrupted || world.state->stop;
    if (stopped) {
        logger_write(&logger, "моделирование прервано пользователем");
    }
    stats_print(&sim, end_reason(&sim));

    // participants read the end of the pipe and exit
    destroy_channels(&sim, down, up);
    free(down);
    free(up);
    sync_destroy(&ready);
    sim_destroy(&sim);
    station_destroy(&station);
    logger_close(&logger);
    world_destroy(&answer);
    world_destroy(&world);
    return stopped ? 128 + SIGINT : 0;
}

static ErrorCode subscribe_all(SimContext* sim, World* world, Station* station) {
    ErrorCode error = ERR_OK;
    for (int i = 0; i < world->state->cars_count && error == ERR_OK; i++) {
        error = sim_subscribe(sim, &world->cars[i], vehicle_on_tick, ORDER_VEHICLE, i);
    }
    if (error == ERR_OK) {
        error = sim_subscribe(sim, station, station_on_tick, ORDER_STATION, 0);
    }
    for (int i = 0; i < world->state->chargers_count && error == ERR_OK; i++) {
        error = sim_subscribe(sim, &world->chargers[i], charger_on_tick, ORDER_CHARGER, i);
    }
    return error;
}

// two pipes per participant, named by its role and index
static ErrorCode create_channels(SimContext* sim, const char* run, Channel* down, Channel* up) {
    for (int i = 0; i < sim->subscribers_count; i++) {
        down[i].fd = -1;
        up[i].fd = -1;
    }
    ErrorCode error = ERR_OK;
    for (int i = 0; i < sim->subscribers_count && error == ERR_OK; i++) {
        TickSubscriber* subscriber = sim->subscribers[i];
        const char* name = ORDER_NAMES[subscriber->order];
        error = channel_create(&down[i], run, name, subscriber->index, "down");
        if (error == ERR_OK) {
            error = channel_create(&up[i], run, name, subscriber->index, "up");
        }
    }
    return error;
}

static void destroy_channels(SimContext* sim, Channel* down, Channel* up) {
    for (int i = 0; i < sim->subscribers_count; i++) {
        channel_destroy(&down[i]);
        channel_destroy(&up[i]);
    }
}

// same order as in the participant, so nobody waits forever, then send the first world
static ErrorCode open_channels(SimContext* sim, const char* run, Channel* down, Channel* up, World* world) {
    ErrorCode error = ERR_OK;
    for (int i = 0; i < sim->subscribers_count && error == ERR_OK; i++) {
        TickSubscriber* subscriber = sim->subscribers[i];
        const char* name = ORDER_NAMES[subscriber->order];
        error = channel_open(&down[i], run, name, subscriber->index, "down", O_WRONLY);
        if (error == ERR_OK) {
            error = channel_open(&up[i], run, name, subscriber->index, "up", O_RDONLY);
        }
        if (error == ERR_OK) {
            error = world_send(&down[i], world);
        }
    }
    return error;
}

// like sim_tick, but each participant gets the world before its turn and sends it back after
static ErrorCode exchange_tick(SimContext* sim, World* world, World* answer, Channel* down, Channel* up) {
    ErrorCode error = ERR_OK;
    for (int phase = 0; phase < ORDER_COUNT && error == ERR_OK; phase++) {
        for (int i = 0; i < sim->subscribers_count && error == ERR_OK; i++) {
            if ((int)sim->subscribers[i]->order == phase) {
                error = world_send(&down[i], world);
                sync_post(&sim->subscribers[i]->start);
            }
        }
        for (int i = 0; i < sim->subscribers_count && error == ERR_OK; i++) {
            if ((int)sim->subscribers[i]->order == phase) {
                error = world_receive(&up[i], answer);
                if (error == ERR_OK) {
                    sync_lock(&sim->phase_done[phase]);
                    apply(sim, world, answer, sim->subscribers[i]);
                }
            }
        }
    }
    world->state->now++;
    return error;
}

// take from the answer only what this participant can change
static void apply(SimContext* sim, World* world, const World* answer, const TickSubscriber* subscriber) {
    SharedState* state = world->state;
    int i = subscriber->index;
    if (subscriber->order == ORDER_VEHICLE) {
        // every vehicle changed its own copy of the queue, so the real one is changed here
        CarState before = world->cars[i].car_state;
        CarState after = answer->cars[i].car_state;
        if (before == CAR_PENDING && after == CAR_QUEUED) {
            station_request_charger(sim->station, i);
        } else if (before == CAR_QUEUED && after == CAR_TIMED_OUT) {
            station_leave_queue(sim->station, i);
        }
        world->cars[i] = answer->cars[i];
    } else if (subscriber->order == ORDER_STATION) {
        memcpy(world->cars, answer->cars, (size_t)state->cars_count * sizeof(Vehicle));
        memcpy(world->chargers, answer->chargers, (size_t)state->chargers_count * sizeof(Charger));
        memcpy(world->queue, answer->queue, (size_t)state->cars_count * sizeof(int));
        state->queue_count = answer->state->queue_count;
    } else {
        int car = world->chargers[i].vehicle_index;
        if (car >= 0) {
            world->cars[car] = answer->cars[car];
        }
        world->chargers[i] = answer->chargers[i];
    }
    if (answer->state->stop) {
        state->stop = 1;
    }
}

static const char* end_reason(const SimContext* sim) {
    if (interrupted || sim->state->stop) {
        return "прервано пользователем";
    }
    if (sim->mode == MODE_LIMITED && sim->state->now >= sim->period_time) {
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
