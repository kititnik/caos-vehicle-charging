#include "sim_context.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#define INITIAL_SUBSCRIBERS_CAPACITY 16

static void* subscriber_thread(void* arg);
static void wait_sem(sem_t* sem);
static void destroy_semaphores(SimContext* sim, int phases);

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
    sim->stopped = 0;
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

    for (int phase = 0; phase < ORDER_COUNT; phase++) {
        sim->phase_threads[phase] = 0;
        if (sem_init(&sim->phase_done[phase], 0, 0) != 0) {
            destroy_semaphores(sim, phase);
            return ERR_SEMAPHORE;
        }
    }
    return ERR_OK;
}

// stops threads if still running, then frees everything
void sim_destroy(SimContext* sim) {
    if (sim == NULL) {
        return;
    }

    sim_stop(sim);
    for (int i = 0; i < sim->subscribers_count; i++) {
        sem_destroy(&sim->subscribers[i]->start);
        free(sim->subscribers[i]);
    }
    free(sim->subscribers);
    sim->subscribers = NULL;
    sim->subscribers_count = 0;
    sim->subscribers_capacity = 0;

    destroy_semaphores(sim, ORDER_COUNT);
}

// start a thread for the participant, it sleeps until its phase begins
ErrorCode sim_subscribe(SimContext* sim, void* self, TickHandler handler, TickOrder order) {
    if (sim == NULL || self == NULL || handler == NULL) {
        return ERR_NULL_ARGUMENT;
    }
    if (order < 0 || order >= ORDER_COUNT) {
        return ERR_INVALID_ARGUMENT;
    }

    if (sim->subscribers_count == sim->subscribers_capacity) {
        if (sim->subscribers_capacity > INT_MAX / 2) {
            return ERR_SUBSCRIBERS_FULL;
        }
        int capacity = sim->subscribers_capacity == 0 ? INITIAL_SUBSCRIBERS_CAPACITY : sim->subscribers_capacity * 2;
        TickSubscriber** subscribers = realloc(sim->subscribers, (size_t) capacity * sizeof(TickSubscriber*));
        if (subscribers == NULL) {
            return ERR_OUT_OF_MEMORY;
        }
        sim->subscribers = subscribers;
        sim->subscribers_capacity = capacity;
    }

    TickSubscriber* subscriber = malloc(sizeof(TickSubscriber));
    if (subscriber == NULL) {
        return ERR_OUT_OF_MEMORY;
    }
    subscriber->self = self;
    subscriber->handler = handler;
    subscriber->order = order;
    subscriber->sim = sim;
    if (sem_init(&subscriber->start, 0, 0) != 0) {
        free(subscriber);
        return ERR_SEMAPHORE;
    }
    if (pthread_create(&subscriber->thread, NULL, subscriber_thread, subscriber) != 0) {
        sem_destroy(&subscriber->start);
        free(subscriber);
        return ERR_THREAD;
    }
    sim->subscribers[sim->subscribers_count] = subscriber;
    sim->subscribers_count++;
    sim->phase_threads[order]++;
    return ERR_OK;
}

// run the phases one by one, in each: release its threads, wait until all finish
void sim_tick(SimContext* sim) {
    for (int phase = 0; phase < ORDER_COUNT; phase++) {
        for (int i = 0; i < sim->subscribers_count; i++) {
            if ((int)sim->subscribers[i]->order == phase) {
                sem_post(&sim->subscribers[i]->start);
            }
        }
        for (int i = 0; i < sim->phase_threads[phase]; i++) {
            wait_sem(&sim->phase_done[phase]);
        }
    }
    sim->now++;
}

// wake every thread with the stop flag set and join them
void sim_stop(SimContext* sim) {
    if (sim->stopped) {
        return;
    }
    sim->stopped = 1;
    for (int i = 0; i < sim->subscribers_count; i++) {
        sem_post(&sim->subscribers[i]->start);
    }
    for (int i = 0; i < sim->subscribers_count; i++) {
        pthread_join(sim->subscribers[i]->thread, NULL);
    }
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

// thread body: wait for the phase signal, do one tick of work, report back
static void* subscriber_thread(void* arg) {
    TickSubscriber* subscriber = (TickSubscriber*)arg;
    SimContext* sim = subscriber->sim;
    while (1) {
        wait_sem(&subscriber->start);
        if (sim->stopped) {
            break;
        }
        subscriber->handler(subscriber->self, sim);
        sem_post(&sim->phase_done[subscriber->order]);
    }
    return NULL;
}

static void wait_sem(sem_t* sem) {
    while (sem_wait(sem) != 0 && errno == EINTR) {
    }
}

// destroy semaphores of the first `phases` phases (the rest were never initialized)
static void destroy_semaphores(SimContext* sim, int phases) {
    for (int phase = 0; phase < phases; phase++) {
        sem_destroy(&sim->phase_done[phase]);
    }
}
