#include "station.h"

#include <stdlib.h>

#include "sim_context.h"

#define EPS 1e-9

static int is_compatible(const Station* station, const Vehicle* vehicle);
static Charger* find_free_charger(Station* station, const Vehicle* vehicle);
static int is_better_charger(const Station* station, const Charger* candidate, const Charger* best);
static void remove_from_queue(Station* station, int index);
static void assign_chargers(Station* station);
static void distribute_power(Station* station);
static void distribute_weighted(Station* station);
static void distribute_priority(Station* station);
static double charger_weight(const Station* station, const Charger* charger);

ErrorCode station_init(Station* station, const Config* config, const Logger* logger) {
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
    station->logger = logger;
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
    Charger* best = NULL;
    for (int i = 0; i < station->chargers_count; i++) {
        Charger* charger = &station->chargers[i];
        if (!charger_is_free(charger) || !charger_is_compatible(charger, vehicle)) {
            continue;
        }
        if (best == NULL || is_better_charger(station, charger, best)) {
            best = charger;
        }
    }
    return best;
}

static int is_better_charger(const Station* station, const Charger* candidate, const Charger* best) {
    switch (station->selection_strategy) {
        case SELECT_MAX_POWER:
            return candidate->max_power > best->max_power;
        case SELECT_MIN_POWER:
            return candidate->max_power < best->max_power;
        default:
            return 0;
    }
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
            logger_write(station->logger, "машине %d назначено устройство %d", vehicle->id, charger->id);
            remove_from_queue(station, i);
        } else {
            i++;
        }
    }
}

static void distribute_power(Station* station) {
    for (int i = 0; i < station->chargers_count; i++) {
        charger_accept_power(&station->chargers[i], 0);
    }
    if (station->distribution_strategy == DIST_PRIORITY) {
        distribute_priority(station);
    } else {
        distribute_weighted(station);
    }
    for (int i = 0; i < station->chargers_count; i++) {
        Charger* charger = &station->chargers[i];
        if (!charger_is_free(charger)) {
            logger_write(station->logger, "устройство %d: выделено %.1f кВт", charger->id, charger->allocated_power);
        }
    }
}

static void distribute_weighted(Station* station) {
    double remaining = station->power_limit;
    int has_leftover = 1;
    while (has_leftover && remaining > EPS) {
        has_leftover = 0;
        double total_weight = 0;
        for (int i = 0; i < station->chargers_count; i++) {
            Charger* charger = &station->chargers[i];
            if (!charger_is_free(charger) && charger->allocated_power < charger_get_max_accepted_power(charger) - EPS) {
                total_weight += charger_weight(station, charger);
            }
        }
        if (total_weight <= EPS) {
            return;
        }

        double given = 0;
        for (int i = 0; i < station->chargers_count; i++) {
            Charger* charger = &station->chargers[i];
            if (charger_is_free(charger) || charger->allocated_power >= charger_get_max_accepted_power(charger) - EPS) {
                continue;
            }
            double before = charger->allocated_power;
            double wanted = before + remaining * charger_weight(station, charger) / total_weight;
            double after = charger_accept_power(charger, wanted);
            given += after - before;
            if (after < wanted - EPS) {
                has_leftover = 1;
            }
        }
        remaining -= given;
    }
}

static void distribute_priority(Station* station) {
    double remaining = station->power_limit;
    while (remaining > EPS) {
        Charger* first = NULL;
        for (int i = 0; i < station->chargers_count; i++) {
            Charger* charger = &station->chargers[i];
            if (charger_is_free(charger) || charger->allocated_power > 0) {
                continue;
            }
            if (first == NULL || charger->vehicle->arrival_time < first->vehicle->arrival_time) {
                first = charger;
            }
        }
        if (first == NULL) {
            return;
        }
        remaining -= charger_accept_power(first, remaining);
    }
}

static double charger_weight(const Station* station, const Charger* charger) {
    if (station->distribution_strategy == DIST_ADAPTIVE) {
        return charger->vehicle->target_charge - charger->vehicle->current_charge;
    }
    return 1;
}
