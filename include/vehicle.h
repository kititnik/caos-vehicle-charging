#ifndef VEHICLE_H
#define VEHICLE_H

#include "car_state.h"
#include "connector.h"

struct SimContext;

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

void vehicle_on_tick(void* self, struct SimContext* sim);

double vehicle_charge(Vehicle* vehicle, double energy);

#endif //VEHICLE_H
