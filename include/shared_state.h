#ifndef SHARED_STATE_H
#define SHARED_STATE_H

#include "distribution_strategy.h"
#include "selection_strategy.h"
#include "run_mode.h"
#include "charger.h"
#include "vehicle.h"
#include "config.h"
#include "shared_memory.h"
#include "error_code.h"

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

// shared memory segments and local pointers to them
typedef struct Shared {
    SharedMemory state_memory;
    SharedMemory cars_memory;
    SharedMemory chargers_memory;
    SharedMemory queue_memory;

    SharedState* state;
    Vehicle* cars;
    Charger* chargers;
    int* queue;
} Shared;

// create all segments and copy config there, on failure nothing is left behind
ErrorCode shared_state_create(Shared* shared, const Config* config, const char* run);
void shared_state_destroy(Shared* shared);

#endif //SHARED_STATE_H
