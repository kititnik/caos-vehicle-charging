#include "vehicle.h"

#include "sim_context.h"

#define EPS 1e-9

// arrive on arrival tick, leave the queue after deadline
void vehicle_on_tick(void* self, SimContext* sim) {
    Vehicle* vehicle = (Vehicle*)self;
    if (vehicle->car_state == CAR_PENDING && vehicle->arrival_time == sim->now) {
        logger_write(sim->logger, "машина %d прибыла", vehicle->id);
        ErrorCode error = station_request_charger(sim->station, vehicle);
        if (error == ERR_OK) {
            logger_write(sim->logger, "машина %d: разъём совместим, встала в очередь", vehicle->id);
        } else if (error == ERR_INCOMPATIBLE) {
            logger_write(sim->logger, "машина %d: нет совместимого разъёма, уезжает", vehicle->id);
        }
    }
    else if (vehicle->car_state == CAR_QUEUED && sim->now >= vehicle->deadline_time) {
        station_leave_queue(sim->station, vehicle);
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
