#ifndef STATION_H
#define STATION_H

#include "charger.h"
#include "vehicle.h"
#include "distribution_strategy.h"
#include "selection_strategy.h"
#include "logger.h"
#include "shared_state.h"
#include "sync.h"
#include "error_code.h"

struct SimContext;

typedef struct Station {
    SharedState* state;

    Charger* chargers;
    int chargers_count;

    Vehicle* cars;

    // indices in cars array, in arrival order
    int* cars_queue;
    int cars_queue_capacity;
    // vehicles join and leave the queue at the same time
    Sync queue_lock;

    double power_limit;
    DistributionStrategy distribution_strategy;
    SelectionStrategy selection_strategy;

    Logger* logger;
} Station;

ErrorCode station_init(Station* station, SharedState* state, Vehicle* cars, Charger* chargers, int* queue,
                       Logger* logger, const char* lock_name);
void station_destroy(Station* station);

void station_on_tick(void* self, struct SimContext* sim);

ErrorCode station_request_charger(Station* station, int index);
ErrorCode station_leave_queue(Station* station, int index);

#endif //STATION_H
