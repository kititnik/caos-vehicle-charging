#ifndef SIM_CONTEXT_H
#define SIM_CONTEXT_H

#include "vehicle.h"
#include "run_mode.h"

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
} SimContext;

#endif //SIM_CONTEXT_H
