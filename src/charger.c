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
    int is_charged = vehicle_charge(vehicle, charger->allocated_power * sim->tick_duration);
    logger_write(sim->logger, "машина %d: заряд %.1f / %.1f кВт·ч", vehicle->id,
                 vehicle->current_charge, vehicle->target_charge);
    if (is_charged) {
        vehicle->car_state = CAR_DONE;
        charger_detach(charger);
        logger_write(sim->logger, "машина %d зарядилась", vehicle->id);
        logger_write(sim->logger, "устройство %d освободилось", charger->id);
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

double charger_get_max_accepted_power(const Charger* charger) {
    if (charger->vehicle == NULL) {
        return 0;
    }
    if (charger->vehicle->max_power < charger->max_power) {
        return charger->vehicle->max_power;
    }
    return charger->max_power;
}

double charger_accept_power(Charger* charger, double power) {
    double max_power = charger_get_max_accepted_power(charger);
    charger->allocated_power = power < max_power ? power : max_power;
    return charger->allocated_power;
}

double charger_get_power(const Charger* charger) {
    return charger->allocated_power;
}
