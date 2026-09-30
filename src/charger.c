#include "charger.h"

#include <stddef.h>

#include "sim_context.h"
#include "vehicle.h"

void charger_on_tick(void* self, SimContext* sim) {
    Charger* charger = (Charger*)self;
    Vehicle* vehicle = charger->vehicle;
    if (vehicle == NULL) {
        return;
    }
    if (vehicle_charge(vehicle, charger->allocated_power * sim->tick_duration)) {
        vehicle->car_state = CAR_DONE;
        charger_detach(charger);
    }
}

int charger_is_free(const Charger* charger) {
    return charger->vehicle == NULL;
}

int charger_is_compatible(const Charger* charger, const Vehicle* vehicle) {
    return (vehicle->connectors & charger->type) != 0;
}

ErrorCode charger_attach(Charger* charger, Vehicle* vehicle) {
    if (!charger_is_free(charger)) {
        return ERR_CHARGER_BUSY;
    }
    if (!charger_is_compatible(charger, vehicle)) {
        return ERR_INCOMPATIBLE;
    }
    charger->vehicle = vehicle;
    vehicle->charger = charger;
    vehicle->car_state = CAR_CHARGING;
    return ERR_OK;
}

void charger_detach(Charger* charger) {
    if (charger->vehicle != NULL) {
        charger->vehicle->charger = NULL;
    }
    charger->vehicle = NULL;
    charger->allocated_power = 0;
}

double charger_accept_power(Charger* charger, double power) {
    double limit = 0;
    if (charger->vehicle != NULL) {
        limit = charger->max_power;
        if (charger->vehicle->max_power < limit) {
            limit = charger->vehicle->max_power;
        }
    }
    charger->allocated_power = power < limit ? power : limit;
    return charger->allocated_power;
}

double charger_get_power(const Charger* charger) {
    return charger->allocated_power;
}
