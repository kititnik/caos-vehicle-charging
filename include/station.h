#ifndef STATION_H
#define STATION_H

#include "charger.h"
#include "vehicle.h"
#include "distribution_strategy.h"
#include "selection_strategy.h"

typedef struct Station {
    Charger* chargers;
    int chargers_count;

    Vehicle** cars_queue;
    int cars_queue_count;

    double power_limit;
    DistributionStrategy distribution_strategy;
    SelectionStrategy selection_strategy;
} Station;

#endif //STATION_H
