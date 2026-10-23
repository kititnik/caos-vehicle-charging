#ifndef SIM_CONTEXT_H
#define SIM_CONTEXT_H

#include <pthread.h>
#include <semaphore.h>

#include "vehicle.h"
#include "station.h"
#include "logger.h"
#include "config.h"
#include "run_mode.h"
#include "tick_order.h"
#include "error_code.h"

typedef struct SimContext SimContext;

// called once per tick from the subscriber's own thread, self is the observer
typedef void (*TickHandler)(void* self, SimContext* sim);

// one participant = one thread, lives until sim_stop
typedef struct TickSubscriber {
    void* self;
    TickHandler handler;
    TickOrder order;
    SimContext* sim;
    pthread_t thread;
    sem_t start;
} TickSubscriber;

typedef struct SimContext {
    double tick_duration;
    int display_delay_ms;

    RunMode mode;
    int period_time;

    int now;
    Vehicle* cars;
    int cars_count;

    TickSubscriber** subscribers;
    int subscribers_count;
    int subscribers_capacity;

    int phase_threads[ORDER_COUNT];
    sem_t phase_done[ORDER_COUNT];
    int stopped;

    Station* station;
    Logger* logger;
} SimContext;

ErrorCode sim_init(SimContext* sim, const Config* config, Station* station, Logger* logger);
void sim_destroy(SimContext* sim);

ErrorCode sim_subscribe(SimContext* sim, void* self, TickHandler handler, TickOrder order);

void sim_tick(SimContext* sim);
void sim_stop(SimContext* sim);
int sim_is_finished(const SimContext* sim);

#endif //SIM_CONTEXT_H
