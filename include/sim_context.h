#ifndef SIM_CONTEXT_H
#define SIM_CONTEXT_H

#include <sys/types.h>

#include "vehicle.h"
#include "station.h"
#include "logger.h"
#include "shared_state.h"
#include "sync.h"
#include "run_mode.h"
#include "tick_order.h"
#include "error_code.h"

typedef struct SimContext SimContext;

// called once per tick from the subscriber's own process, self is the observer
typedef void (*TickHandler)(void* self, SimContext* sim);

// one participant = one child process, lives until sim_stop
typedef struct TickSubscriber {
    void* self;
    TickHandler handler;
    TickOrder order;
    pid_t pid;
    Sync start;
} TickSubscriber;

typedef struct SimContext {
    SharedState* state;

    double tick_duration;
    int display_delay_ms;

    RunMode mode;
    int period_time;

    Vehicle* cars;
    int cars_count;

    TickSubscriber** subscribers;
    int subscribers_count;
    int subscribers_capacity;

    int phase_processes[ORDER_COUNT];
    Sync phase_done[ORDER_COUNT];
    int stopped;

    // part of every semaphore name
    const char* run;

    Station* station;
    Logger* logger;
} SimContext;

ErrorCode sim_init(SimContext* sim, SharedState* state, Vehicle* cars, Station* station, Logger* logger,
                   const char* run);
void sim_destroy(SimContext* sim);

ErrorCode sim_subscribe(SimContext* sim, void* self, TickHandler handler, TickOrder order);
ErrorCode sim_start(SimContext* sim);

void sim_tick(SimContext* sim);
void sim_stop(SimContext* sim);
int sim_is_finished(const SimContext* sim);

#endif //SIM_CONTEXT_H
