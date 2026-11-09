#include "world.h"

#include <stdlib.h>
#include <string.h>

static ErrorCode allocate(World* world, int cars_count, int chargers_count);

ErrorCode world_create(World* world, const Config* config) {
    memset(world, 0, sizeof(*world));
    world->state = calloc(1, sizeof(SharedState));
    if (world->state == NULL || allocate(world, config->cars_count, config->chargers_count) != ERR_OK) {
        world_destroy(world);
        return ERR_OUT_OF_MEMORY;
    }
    shared_state_fill(world->state, world->cars, world->chargers, config);
    return ERR_OK;
}

void world_destroy(World* world) {
    free(world->state);
    free(world->cars);
    free(world->chargers);
    free(world->queue);
    memset(world, 0, sizeof(*world));
}

ErrorCode world_send(Channel* channel, const World* world) {
    const SharedState* state = world->state;
    ErrorCode error = channel_write(channel, state, sizeof(SharedState));
    if (error == ERR_OK) {
        error = channel_write(channel, world->cars, (size_t)state->cars_count * sizeof(Vehicle));
    }
    if (error == ERR_OK) {
        error = channel_write(channel, world->chargers, (size_t)state->chargers_count * sizeof(Charger));
    }
    if (error == ERR_OK) {
        error = channel_write(channel, world->queue, (size_t)state->cars_count * sizeof(int));
    }
    return error;
}

ErrorCode world_receive(Channel* channel, World* world) {
    if (world->state == NULL) {
        world->state = calloc(1, sizeof(SharedState));
        if (world->state == NULL) {
            return ERR_OUT_OF_MEMORY;
        }
    }
    SharedState* state = world->state;
    ErrorCode error = channel_read(channel, state, sizeof(SharedState));
    if (error == ERR_OK && world->cars == NULL) {
        error = allocate(world, state->cars_count, state->chargers_count);
    }
    if (error == ERR_OK) {
        error = channel_read(channel, world->cars, (size_t)state->cars_count * sizeof(Vehicle));
    }
    if (error == ERR_OK) {
        error = channel_read(channel, world->chargers, (size_t)state->chargers_count * sizeof(Charger));
    }
    if (error == ERR_OK) {
        error = channel_read(channel, world->queue, (size_t)state->cars_count * sizeof(int));
    }
    return error;
}

static ErrorCode allocate(World* world, int cars_count, int chargers_count) {
    world->cars = calloc((size_t)cars_count, sizeof(Vehicle));
    world->chargers = calloc((size_t)chargers_count, sizeof(Charger));
    world->queue = calloc((size_t)cars_count, sizeof(int));
    if (world->cars == NULL || world->chargers == NULL || world->queue == NULL) {
        return ERR_OUT_OF_MEMORY;
    }
    return ERR_OK;
}
