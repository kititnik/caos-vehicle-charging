#ifndef SHARED_STATE_H
#define SHARED_STATE_H

#include "distribution_strategy.h"
#include "selection_strategy.h"
#include "run_mode.h"

typedef struct SharedState {
    double power_limit;
    int max_wait_time;
    DistributionStrategy distribution_strategy;
    SelectionStrategy selection_strategy;
    double tick_duration;
    int display_delay_ms;
    RunMode mode;
    int period_time;

    int cars_count;
    int chargers_count;

    int queue_count;

    int now;
    int stop;
} SharedState;

#endif //SHARED_STATE_H
