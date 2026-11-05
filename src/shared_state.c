#include "shared_state.h"

#include <string.h>

#include "sync.h"

static ErrorCode create_segment(SharedMemory* memory, const char* run, const char* base, size_t size);

ErrorCode shared_state_create(Shared* shared, const Config* config, const char* run) {
    size_t cars_size = (size_t)config->cars_count * sizeof(Vehicle);
    size_t chargers_size = (size_t)config->chargers_count * sizeof(Charger);
    size_t queue_size = (size_t)config->cars_count * sizeof(int);

    memset(shared, 0, sizeof(*shared));
    if (create_segment(&shared->state_memory, run, "state", sizeof(SharedState)) != ERR_OK ||
        create_segment(&shared->cars_memory, run, "cars", cars_size) != ERR_OK ||
        create_segment(&shared->chargers_memory, run, "chargers", chargers_size) != ERR_OK ||
        create_segment(&shared->queue_memory, run, "queue", queue_size) != ERR_OK) {
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
    state->cars_count = config->cars_count;
    state->chargers_count = config->chargers_count;

    memcpy(shared->cars, config->cars, cars_size);
    memcpy(shared->chargers, config->chargers, chargers_size);
    return ERR_OK;
}

void shared_state_destroy(Shared* shared) {
    shared_memory_destroy(&shared->state_memory);
    shared_memory_destroy(&shared->cars_memory);
    shared_memory_destroy(&shared->chargers_memory);
    shared_memory_destroy(&shared->queue_memory);
}

static ErrorCode create_segment(SharedMemory* memory, const char* run, const char* base, size_t size) {
    char name[SHARED_MEMORY_NAME_SIZE];
    sync_make_name(name, sizeof(name), run, base, 0);
    return shared_memory_create(memory, name, size);
}
