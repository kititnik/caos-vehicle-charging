#ifndef SHARED_STATE_H
#define SHARED_STATE_H

#include <limits.h>
#include <sys/types.h>

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
    // empty if no log file
    char log_path[PATH_MAX];

    int cars_count;
    int chargers_count;

    int queue_count;

    int now;
    int stop;
    // process that created the state, if it is gone the objects are stale
    pid_t owner_pid;
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
// map segments created by another process, ERR_NOT_FOUND if not there yet or the owner is dead
ErrorCode shared_state_open(Shared* shared, const char* run);
void shared_state_close(Shared* shared);

#endif //SHARED_STATE_H
