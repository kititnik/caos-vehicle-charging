#ifndef STATION_H
#define STATION_H

#include <pthread.h>

#include "charger.h"
#include "vehicle.h"
#include "distribution_strategy.h"
#include "selection_strategy.h"
#include "logger.h"
#include "config.h"
#include "error_code.h"

struct SimContext;

typedef struct Station {
    Charger* chargers;
    int chargers_count;

    Vehicle** cars_queue;
    int cars_queue_count;
    int cars_queue_capacity;
    // vehicle threads join and leave the queue at the same time
    pthread_mutex_t queue_lock;

    double power_limit;
    DistributionStrategy distribution_strategy;
    SelectionStrategy selection_strategy;

    Logger* logger;
} Station;

ErrorCode station_init(Station* station, const Config* config, Logger* logger);
void station_destroy(Station* station);

void station_on_tick(void* self, struct SimContext* sim);

ErrorCode station_request_charger(Station* station, Vehicle* vehicle);
ErrorCode station_leave_queue(Station* station, Vehicle* vehicle);

#endif //STATION_H
