#ifndef STATION_H
#define STATION_H

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

    double power_limit;
    DistributionStrategy distribution_strategy;
    SelectionStrategy selection_strategy;
} Station;

ErrorCode station_init(Station* station, const Config* config);
void station_destroy(Station* station);

void station_on_tick(void* self, struct SimContext* sim);

ErrorCode station_request_charger(Station* station, Vehicle* vehicle, const Logger* logger, int now);
ErrorCode station_leave_queue(Station* station, Vehicle* vehicle);

#endif //STATION_H
