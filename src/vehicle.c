#include "vehicle.h"

#include <limits.h>

#include "sim_context.h"

#define EPS 1e-9

void vehicle_on_tick(void* self, SimContext* sim) {
    Vehicle* vehicle = (Vehicle*)self;
    vehicle_act(vehicle, vehicle_decide(vehicle, sim->now), sim);
}

// arrive on arrival tick, leave the queue after deadline
VehicleAction vehicle_decide(const Vehicle* vehicle, int now) {
    if (vehicle->car_state == CAR_PENDING && vehicle->arrival_time <= now) {
        return ACTION_ARRIVE;
    }
    if (vehicle->car_state == CAR_QUEUED && now >= vehicle->deadline_time) {
        return ACTION_LEAVE;
    }
    return ACTION_NONE;
}

// station side of the decision, action that does not fit the state is ignored
void vehicle_act(Vehicle* vehicle, VehicleAction action, SimContext* sim) {
    if (action == ACTION_ARRIVE && vehicle->car_state == CAR_PENDING) {
        int wait = vehicle->deadline_time - vehicle->arrival_time;
        vehicle->deadline_time = sim->now > INT_MAX - wait ? INT_MAX : sim->now + wait;
        vehicle->arrival_time = sim->now;
        logger_write(sim->logger, "машина %d прибыла", vehicle->id);
        ErrorCode error = station_request_charger(sim->station, vehicle->index);
        if (error == ERR_OK) {
            logger_write(sim->logger, "машина %d: разъём совместим, встала в очередь", vehicle->id);
        } else if (error == ERR_INCOMPATIBLE) {
            logger_write(sim->logger, "машина %d: нет совместимого разъёма, уезжает", vehicle->id);
        }
    }
    else if (action == ACTION_LEAVE && vehicle->car_state == CAR_QUEUED) {
        station_leave_queue(sim->station, vehicle->index);
        vehicle->car_state = CAR_TIMED_OUT;
        logger_write(sim->logger, "машина %d ушла из очереди по тайм-ауту", vehicle->id);
    }
}

// add energy, clamp to target, returns 1 when full
int vehicle_charge(Vehicle* vehicle, double energy) {
    vehicle->current_charge += energy;
    if (vehicle->current_charge >= vehicle->target_charge - EPS) {
        vehicle->current_charge = vehicle->target_charge;
        return 1;
    }
    return 0;
}
