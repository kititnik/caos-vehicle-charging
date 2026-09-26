#ifndef SIM_CONTEXT_H
#define SIM_CONTEXT_H

#include "vehicle.h"
#include "station.h"
#include "logger.h"
#include "run_mode.h"
#include "tick_order.h"
#include "error_code.h"

typedef struct SimContext SimContext;

typedef void (*TickHandler)(void* self, SimContext* sim);

typedef struct TickSubscriber {
    void* self;
    TickHandler handler;
    TickOrder order;
} TickSubscriber;

typedef struct SimContext {
    int arrival_min_time;
    int arrival_max_time;
    int max_wait_time;

    double tick_duration;
    int display_delay_ms;

    RunMode mode;
    int period_time;

    int cars_count;
    unsigned int seed;

    int now;
    Vehicle* cars;

    TickSubscriber* subscribers;
    int subscribers_count;
    int subscribers_capacity;

    Station* station;
    Logger* logger;
} SimContext;

ErrorCode sim_init(SimContext* sim, int argc, char** argv);
void sim_destroy(SimContext* sim);

ErrorCode sim_subscribe(SimContext* sim, void* self, TickHandler handler, TickOrder order);

void sim_tick(SimContext* sim);
int sim_is_finished(const SimContext* sim);

#endif //SIM_CONTEXT_H
