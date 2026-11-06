#include "shared_state.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

ErrorCode shared_state_create(Shared* shared, const Config* config, const char* run) {
    size_t cars_size = (size_t)config->cars_count * sizeof(Vehicle);
    size_t chargers_size = (size_t)config->chargers_count * sizeof(Charger);
    size_t queue_size = (size_t)config->cars_count * sizeof(int);

    memset(shared, 0, sizeof(*shared));
    if (shared_memory_create(&shared->state_memory, run, "state", sizeof(SharedState)) != ERR_OK ||
        shared_memory_create(&shared->cars_memory, run, "cars", cars_size) != ERR_OK ||
        shared_memory_create(&shared->chargers_memory, run, "chargers", chargers_size) != ERR_OK ||
        shared_memory_create(&shared->queue_memory, run, "queue", queue_size) != ERR_OK) {
        shared_state_destroy(shared);
        return ERR_SHARED_MEMORY;
    }
    shared->state = shared->state_memory.data;
    shared->cars = shared->cars_memory.data;
    shared->chargers = shared->chargers_memory.data;
    shared->queue = shared->queue_memory.data;

    SharedState* state = shared->state;
    state->power_limit = config->power_limit;
    state->max_wait_time = config->max_wait_time;
    state->distribution_strategy = config->distribution_strategy;
    state->selection_strategy = config->selection_strategy;
    state->tick_duration = config->tick_duration;
    state->display_delay_ms = config->display_delay_ms;
    state->mode = config->mode;
    state->period_time = config->period_time;
    state->owner_pid = getpid();
    if (config->log_path != NULL) {
        snprintf(state->log_path, sizeof(state->log_path), "%s", config->log_path);
    }
    state->cars_count = config->cars_count;
    state->chargers_count = config->chargers_count;

    memcpy(shared->cars, config->cars, cars_size);
    memcpy(shared->chargers, config->chargers, chargers_size);
    return ERR_OK;
}

ErrorCode shared_state_open(Shared* shared, const char* run) {
    memset(shared, 0, sizeof(*shared));
    ErrorCode error = shared_memory_open(&shared->state_memory, run, "state");
    if (error == ERR_OK) {
        error = shared_memory_open(&shared->cars_memory, run, "cars");
    }
    if (error == ERR_OK) {
        error = shared_memory_open(&shared->chargers_memory, run, "chargers");
    }
    if (error == ERR_OK) {
        error = shared_memory_open(&shared->queue_memory, run, "queue");
    }
    if (error != ERR_OK) {
        shared_state_close(shared);
        return error;
    }
    shared->state = shared->state_memory.data;
    shared->cars = shared->cars_memory.data;
    shared->chargers = shared->chargers_memory.data;
    shared->queue = shared->queue_memory.data;

    // left by a coordinator that crashed, a new one will recreate everything
    if (kill(shared->state->owner_pid, 0) != 0 && errno == ESRCH) {
        shared_state_close(shared);
        return ERR_NOT_FOUND;
    }
    return ERR_OK;
}

void shared_state_close(Shared* shared) {
    shared_memory_close(&shared->state_memory);
    shared_memory_close(&shared->cars_memory);
    shared_memory_close(&shared->chargers_memory);
    shared_memory_close(&shared->queue_memory);
}

void shared_state_destroy(Shared* shared) {
    shared_memory_destroy(&shared->state_memory);
    shared_memory_destroy(&shared->cars_memory);
    shared_memory_destroy(&shared->chargers_memory);
    shared_memory_destroy(&shared->queue_memory);
}
