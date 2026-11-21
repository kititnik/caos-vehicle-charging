#ifndef VEHICLE_H
#define VEHICLE_H

#include "car_state.h"
#include "connector.h"

struct SimContext;

// what a car wants to do in its turn
typedef enum {
    ACTION_NONE,
    ACTION_ARRIVE,
    ACTION_LEAVE
} VehicleAction;

typedef struct Vehicle {
    int id;
    // position in cars array
    int index;
    CarState car_state;

    double current_charge;
    double capacity;
    double target_charge;
    double max_power;
    unsigned int connectors;

    int arrival_time;
    int deadline_time;
} Vehicle;

void vehicle_on_tick(void* self, struct SimContext* sim);

VehicleAction vehicle_decide(const Vehicle* vehicle, int now);
void vehicle_act(Vehicle* vehicle, VehicleAction action, struct SimContext* sim);

int vehicle_charge(Vehicle* vehicle, double energy);

#endif //VEHICLE_H
