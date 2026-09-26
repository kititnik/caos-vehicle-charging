#ifndef VEHICLE_H
#define VEHICLE_H

#include "car_state.h"

typedef struct Vehicle {
    int id;
    CarState car_state;

    double current_charge;
    double capacity;
    double target_charge;
    double max_power;
    unsigned int connectors;

    int arrival_time;
    int deadline_time;

    struct Charger* charger;
} Vehicle;

#endif //VEHICLE_H
