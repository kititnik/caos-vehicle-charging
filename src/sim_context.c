#include "sim_context.h"

#include <limits.h>
#include <stdlib.h>

#define INITIAL_SUBSCRIBERS_CAPACITY 16

// copy settings from config and compute queue deadlines
ErrorCode sim_init(SimContext* sim, const Config* config, Station* station, Logger* logger) {
    if (sim == NULL || config == NULL || station == NULL) {
        return ERR_NULL_ARGUMENT;
    }

    sim->tick_duration = config->tick_duration;
    sim->display_delay_ms = config->display_delay_ms;
    sim->mode = config->mode;
    sim->period_time = config->period_time;
    sim->now = 0;
    sim->cars = config->cars;
    sim->cars_count = config->cars_count;
    sim->subscribers = NULL;
    sim->subscribers_count = 0;
    sim->subscribers_capacity = 0;
    sim->station = station;
    sim->logger = logger;

    for (int i = 0; i < sim->cars_count; i++) {
        Vehicle* vehicle = &sim->cars[i];
        if (vehicle->arrival_time > INT_MAX - config->max_wait_time) {
            vehicle->deadline_time = INT_MAX;
        } else {
            vehicle->deadline_time = vehicle->arrival_time + config->max_wait_time;
        }
    }
    return ERR_OK;
}

void sim_destroy(SimContext* sim) {
    if (sim == NULL) {
        return;
    }

    free(sim->subscribers);
    sim->subscribers = NULL;
    sim->subscribers_count = 0;
    sim->subscribers_capacity = 0;
}

// add a tick handler, list is sorted by order
ErrorCode sim_subscribe(SimContext* sim, void* self, TickHandler handler, TickOrder order) {
    if (sim == NULL || self == NULL || handler == NULL) {
        return ERR_NULL_ARGUMENT;
    }

    if (sim->subscribers_count == sim->subscribers_capacity) {
        if (sim->subscribers_capacity > INT_MAX / 2) {
            return ERR_SUBSCRIBERS_FULL;
        }
        int capacity = sim->subscribers_capacity == 0 ? INITIAL_SUBSCRIBERS_CAPACITY : sim->subscribers_capacity * 2;
        TickSubscriber* subscribers = realloc(sim->subscribers, (size_t) capacity * sizeof(TickSubscriber));
        if (subscribers == NULL) {
            return ERR_OUT_OF_MEMORY;
        }
        sim->subscribers = subscribers;
        sim->subscribers_capacity = capacity;
    }

    // insertion sort step, shift bigger orders right
    int i = sim->subscribers_count;
    while (i > 0 && sim->subscribers[i - 1].order > order) {
        sim->subscribers[i] = sim->subscribers[i - 1];
        i--;
    }
    sim->subscribers[i].self = self;
    sim->subscribers[i].handler = handler;
    sim->subscribers[i].order = order;
    sim->subscribers_count++;
    return ERR_OK;
}

// call every handler once, then move time forward
void sim_tick(SimContext* sim) {
    for (int i = 0; i < sim->subscribers_count; i++) {
        sim->subscribers[i].handler(sim->subscribers[i].self, sim);
    }
    sim->now++;
}

// done when period is over or no car is still waiting or charging
int sim_is_finished(const SimContext* sim) {
    if (sim->mode == MODE_LIMITED && sim->now >= sim->period_time) {
        return 1;
    }
    for (int i = 0; i < sim->cars_count; i++) {
        CarState state = sim->cars[i].car_state;
        if (state == CAR_PENDING || state == CAR_QUEUED || state == CAR_CHARGING) {
            return 0;
        }
    }
    return 1;
}
