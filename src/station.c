#include "station.h"

#include <stdlib.h>

#include "sim_context.h"

static int is_compatible(const Station* station, const Vehicle* vehicle);
static Charger* find_free_charger(Station* station, const Vehicle* vehicle);
static void remove_from_queue(Station* station, int index);
static void assign_chargers(Station* station);
static void distribute_power(Station* station);

ErrorCode station_init(Station* station, const Config* config) {
    if (station == NULL || config == NULL) {
        return ERR_NULL_ARGUMENT;
    }
    station->cars_queue = malloc((size_t) config->cars_count * sizeof(Vehicle*));
    if (station->cars_queue == NULL) {
        return ERR_OUT_OF_MEMORY;
    }
    station->cars_queue_count = 0;
    station->cars_queue_capacity = config->cars_count;
    station->chargers = config->chargers;
    station->chargers_count = config->chargers_count;
    station->power_limit = config->power_limit;
    station->distribution_strategy = config->distribution_strategy;
    station->selection_strategy = config->selection_strategy;
    return ERR_OK;
}

void station_destroy(Station* station) {
    if (station == NULL) {
        return;
    }
    free(station->cars_queue);
    station->cars_queue = NULL;
    station->cars_queue_count = 0;
    station->cars_queue_capacity = 0;
}

void station_on_tick(void* self, SimContext* sim) {
    Station* station = (Station*)self;
    assign_chargers(station);
    distribute_power(station);
}

ErrorCode station_request_charger(Station* station, Vehicle* vehicle) {
    if (!is_compatible(station, vehicle)) {
        vehicle->car_state = CAR_INCOMPATIBLE;
        return ERR_INCOMPATIBLE;
    }
    if (station->cars_queue_count == station->cars_queue_capacity) {
        return ERR_QUEUE_FULL;
    }
    station->cars_queue[station->cars_queue_count] = vehicle;
    station->cars_queue_count++;
    vehicle->car_state = CAR_QUEUED;
    return ERR_OK;
}

ErrorCode station_leave_queue(Station* station, Vehicle* vehicle) {
    for (int i = 0; i < station->cars_queue_count; i++) {
        if (station->cars_queue[i] == vehicle) {
            remove_from_queue(station, i);
            return ERR_OK;
        }
    }
    return ERR_NOT_IN_QUEUE;
}

static int is_compatible(const Station* station, const Vehicle* vehicle) {
    for (int i = 0; i < station->chargers_count; i++) {
        if (charger_is_compatible(&station->chargers[i], vehicle)) {
            return 1;
        }
    }
    return 0;
}

static Charger* find_free_charger(Station* station, const Vehicle* vehicle) {
    for (int i = 0; i < station->chargers_count; i++) {
        Charger* charger = &station->chargers[i];
        if (charger_is_free(charger) && charger_is_compatible(charger, vehicle)) {
            return charger;
        }
    }
    return NULL;
}

static void remove_from_queue(Station* station, int index) {
    for (int i = index; i < station->cars_queue_count - 1; i++) {
        station->cars_queue[i] = station->cars_queue[i + 1];
    }
    station->cars_queue_count--;
}

static void assign_chargers(Station* station) {
    int i = 0;
    while (i < station->cars_queue_count) {
        Vehicle* vehicle = station->cars_queue[i];
        Charger* charger = find_free_charger(station, vehicle);
        if (charger != NULL && charger_attach(charger, vehicle) == ERR_OK) {
            remove_from_queue(station, i);
        } else {
            i++;
        }
    }
}

static void distribute_power(Station* station) {
    int active_count = 0;
    for (int i = 0; i < station->chargers_count; i++) {
        if (!charger_is_free(&station->chargers[i])) {
            active_count++;
        }
    }

    double remaining = station->power_limit;
    for (int i = 0; i < station->chargers_count; i++) {
        Charger* charger = &station->chargers[i];
        if (charger_is_free(charger)) {
            continue;
        }
        remaining -= charger_accept_power(charger, remaining / active_count);
        active_count--;
    }
}
