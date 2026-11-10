#include "sim_context.h"

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#define INITIAL_SUBSCRIBERS_CAPACITY 16

const char* const ORDER_NAMES[ORDER_COUNT] = {"vehicle", "station", "charger"};

static void release(SimContext* sim, Sync* sem);
static void destroy_semaphores(SimContext* sim, int phases);

// copy settings from shared state, owner computes queue deadlines and creates phase semaphores
ErrorCode sim_init(SimContext* sim, SharedState* state, Vehicle* cars, Station* station, Logger* logger,
                   const char* run, int owner) {
    if (sim == NULL || state == NULL || cars == NULL || station == NULL || run == NULL) {
        return ERR_NULL_ARGUMENT;
    }

    sim->state = state;
    sim->tick_duration = state->tick_duration;
    sim->display_delay_ms = state->display_delay_ms;
    sim->mode = state->mode;
    sim->period_time = state->period_time;
    sim->cars = cars;
    sim->cars_count = state->cars_count;
    sim->subscribers = NULL;
    sim->subscribers_count = 0;
    sim->subscribers_capacity = 0;
    sim->stopped = 0;
    sim->run = run;
    sim->owner = owner;
    sim->station = station;
    sim->logger = logger;

    for (int i = 0; owner && i < sim->cars_count; i++) {
        Vehicle* vehicle = &sim->cars[i];
        if (vehicle->arrival_time > INT_MAX - state->max_wait_time) {
            vehicle->deadline_time = INT_MAX;
        } else {
            vehicle->deadline_time = vehicle->arrival_time + state->max_wait_time;
        }
    }

    for (int phase = 0; phase < ORDER_COUNT; phase++) {
        sim->phase_processes[phase] = 0;
        ErrorCode error = owner ? sync_create(&sim->phase_done[phase], run, "phase", phase, 0)
                                : sync_open(&sim->phase_done[phase], run, "phase", phase);
        if (error != ERR_OK) {
            destroy_semaphores(sim, phase);
            return error;
        }
    }
    return ERR_OK;
}

// owner stops processes if still running, then removes everything
void sim_destroy(SimContext* sim) {
    if (sim == NULL) {
        return;
    }

    if (sim->owner) {
        sim_stop(sim);
    }
    for (int i = 0; i < sim->subscribers_count; i++) {
        release(sim, &sim->subscribers[i]->start);
        free(sim->subscribers[i]);
    }
    free(sim->subscribers);
    sim->subscribers = NULL;
    sim->subscribers_count = 0;
    sim->subscribers_capacity = 0;

    destroy_semaphores(sim, ORDER_COUNT);
}

// remember the participant and create or open its start semaphore
ErrorCode sim_subscribe(SimContext* sim, void* self, TickHandler handler, TickOrder order, int index) {
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
    subscriber->index = index;
    subscriber->pid = -1;
    ErrorCode error = sim->owner ? sync_create(&subscriber->start, sim->run, ORDER_NAMES[order], index, 0)
                                 : sync_open(&subscriber->start, sim->run, ORDER_NAMES[order], index);
    if (error != ERR_OK) {
        free(subscriber);
        return error;
    }
    sim->subscribers[sim->subscribers_count] = subscriber;
    sim->subscribers_count++;
    sim->phase_processes[order]++;
    return ERR_OK;
}

// fork every participant
ErrorCode sim_start(SimContext* sim) {
    for (int i = 0; i < sim->subscribers_count; i++) {
        TickSubscriber* subscriber = sim->subscribers[i];
        pid_t pid = fork();
        if (pid < 0) {
            sim_stop(sim);
            return ERR_PROCESS;
        }
        if (pid == 0) {
            signal(SIGINT, SIG_IGN);
            sim_run_subscriber(sim, subscriber);
            _exit(0);
        }
        subscriber->pid = pid;
    }
    return ERR_OK;
}

// run the phases one by one, in each: release its processes, wait until all finish
void sim_tick(SimContext* sim) {
    for (int phase = 0; phase < ORDER_COUNT; phase++) {
        for (int i = 0; i < sim->subscribers_count; i++) {
            if ((int)sim->subscribers[i]->order == phase) {
                sync_post(&sim->subscribers[i]->start);
            }
        }
        for (int i = 0; i < sim->phase_processes[phase]; i++) {
            sync_lock(&sim->phase_done[phase]);
        }
    }
    sim->state->now++;
}

// wake every participant with the stop flag set, wait for the forked ones
void sim_stop(SimContext* sim) {
    if (sim->stopped) {
        return;
    }
    sim->stopped = 1;
    sim->state->stop = 1;
    for (int i = 0; i < sim->subscribers_count; i++) {
        sync_post(&sim->subscribers[i]->start);
    }
    for (int i = 0; i < sim->subscribers_count; i++) {
        TickSubscriber* subscriber = sim->subscribers[i];
        if (subscriber->pid <= 0) {
            continue;
        }
        while (waitpid(subscriber->pid, NULL, 0) < 0 && errno == EINTR) {
        }
        subscriber->pid = -1;
    }
}

// participant body: wait for the phase signal, do one tick of work, report back
void sim_run_subscriber(SimContext* sim, TickSubscriber* subscriber) {
    while (sync_lock(&subscriber->start) == ERR_OK && !sim->state->stop) {
        subscriber->handler(subscriber->self, sim);
        sync_post(&sim->phase_done[subscriber->order]);
    }
}

// done when period is over or no car is still waiting or charging
int sim_is_finished(const SimContext* sim) {
    if (sim->mode == MODE_LIMITED && sim->state->now >= sim->period_time) {
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

// owner removes the semaphore, participant only closes it
static void release(SimContext* sim, Sync* sem) {
    if (sim->owner) {
        sync_destroy(sem);
    } else {
        sync_close(sem);
    }
}

// release semaphores of the first `phases` phases (the rest were never opened)
static void destroy_semaphores(SimContext* sim, int phases) {
    for (int phase = 0; phase < phases; phase++) {
        release(sim, &sim->phase_done[phase]);
    }
}
