#include "station.h"

#include <stddef.h>

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
static const Vehicle* attached_vehicle(const Station* station, const Charger* charger);

ErrorCode station_init(Station* station, SharedState* state, Vehicle* cars, Charger* chargers, int* queue,
                       Logger* logger, const char* run) {
    if (station == NULL || state == NULL || cars == NULL || chargers == NULL || queue == NULL) {
        return ERR_NULL_ARGUMENT;
    }
    if (sync_create(&station->queue_lock, run, "queue", 0, 1) != ERR_OK) {
        return ERR_SEMAPHORE;
    }
    station->state = state;
    station->cars_queue = queue;
    station->cars_queue_capacity = state->cars_count;
    station->cars = cars;
    station->chargers = chargers;
    station->chargers_count = state->chargers_count;
    station->power_limit = state->power_limit;
    station->distribution_strategy = state->distribution_strategy;
    station->selection_strategy = state->selection_strategy;
    station->logger = logger;
    return ERR_OK;
}

void station_destroy(Station* station) {
    if (station == NULL) {
        return;
    }
    sync_destroy(&station->queue_lock);
}

// every tick: plug in waiting cars, then split power
void station_on_tick(void* self, SimContext* sim) {
    Station* station = (Station*)self;
    assign_chargers(station);
    distribute_power(station);
}

// put car in queue, or reject if no charger here fits at all
// called from vehicle processes, the queue is guarded by the lock
ErrorCode station_request_charger(Station* station, int index) {
    Vehicle* vehicle = &station->cars[index];
    if (!is_compatible(station, vehicle)) {
        vehicle->car_state = CAR_INCOMPATIBLE;
        return ERR_INCOMPATIBLE;
    }
    sync_lock(&station->queue_lock);
    if (station->state->queue_count == station->cars_queue_capacity) {
        sync_post(&station->queue_lock);
        return ERR_QUEUE_FULL;
    }
    station->cars_queue[station->state->queue_count] = index;
    station->state->queue_count++;
    vehicle->car_state = CAR_QUEUED;
    sync_post(&station->queue_lock);
    return ERR_OK;
}

// called from vehicle processes, the queue is guarded by the lock
ErrorCode station_leave_queue(Station* station, int index) {
    ErrorCode result = ERR_NOT_IN_QUEUE;
    sync_lock(&station->queue_lock);
    for (int i = 0; i < station->state->queue_count; i++) {
        if (station->cars_queue[i] == index) {
            remove_from_queue(station, i);
            result = ERR_OK;
            break;
        }
    }
    sync_post(&station->queue_lock);
    return result;
}

// 1 if any charger here fits the car
static int is_compatible(const Station* station, const Vehicle* vehicle) {
    for (int i = 0; i < station->chargers_count; i++) {
        if (charger_is_compatible(&station->chargers[i], vehicle)) {
            return 1;
        }
    }
    return 0;
}

// pick a free fitting charger using selection strategy
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
    for (int i = index; i < station->state->queue_count - 1; i++) {
        station->cars_queue[i] = station->cars_queue[i + 1];
    }
    station->state->queue_count--;
}

// walk the queue in order, plug in whoever fits
static void assign_chargers(Station* station) {
    int i = 0;
    while (i < station->state->queue_count) {
        int index = station->cars_queue[i];
        Vehicle* vehicle = &station->cars[index];
        Charger* charger = find_free_charger(station, vehicle);
        if (charger != NULL && charger_attach(charger, vehicle) == ERR_OK) {
            logger_write(station->logger, "машине %d назначено устройство %d", vehicle->id, charger->id);
            remove_from_queue(station, i);
        } else {
            i++;
        }
    }
}

// reset all power, run the strategy, log the result
static void distribute_power(Station* station) {
    for (int i = 0; i < station->chargers_count; i++) {
        charger_accept_power(&station->chargers[i], NULL, 0);
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

// split by weight, leftovers from capped chargers go round again
static void distribute_weighted(Station* station) {
    double remaining = station->power_limit;
    int has_leftover = 1;
    while (has_leftover && remaining > EPS) {
        has_leftover = 0;
        double total_weight = 0;
        for (int i = 0; i < station->chargers_count; i++) {
            Charger* charger = &station->chargers[i];
            if (!charger_is_free(charger) &&
                charger->allocated_power < charger_get_max_accepted_power(charger, attached_vehicle(station, charger)) - EPS) {
                total_weight += charger_weight(station, charger);
            }
        }
        if (total_weight <= EPS) {
            return;
        }

        double given = 0;
        for (int i = 0; i < station->chargers_count; i++) {
            Charger* charger = &station->chargers[i];
            const Vehicle* vehicle = attached_vehicle(station, charger);
            if (vehicle == NULL || charger->allocated_power >= charger_get_max_accepted_power(charger, vehicle) - EPS) {
                continue;
            }
            double before = charger->allocated_power;
            double wanted = before + remaining * charger_weight(station, charger) / total_weight;
            double after = charger_accept_power(charger, vehicle, wanted);
            given += after - before;
            // hit the cap, so some power is left to hand out
            if (after < wanted - EPS) {
                has_leftover = 1;
            }
        }
        remaining -= given;
    }
}

// earliest arrival takes all it can, then the next one
static void distribute_priority(Station* station) {
    double remaining = station->power_limit;
    while (remaining > EPS) {
        Charger* first = NULL;
        for (int i = 0; i < station->chargers_count; i++) {
            Charger* charger = &station->chargers[i];
            if (charger_is_free(charger) || charger->allocated_power > 0) {
                continue;
            }
            if (first == NULL ||
                attached_vehicle(station, charger)->arrival_time < attached_vehicle(station, first)->arrival_time) {
                first = charger;
            }
        }
        if (first == NULL) {
            return;
        }
        remaining -= charger_accept_power(first, attached_vehicle(station, first), remaining);
    }
}

// uniform: 1 for each, adaptive: more for cars that need more energy
static double charger_weight(const Station* station, const Charger* charger) {
    if (station->distribution_strategy == DIST_ADAPTIVE) {
        const Vehicle* vehicle = attached_vehicle(station, charger);
        return vehicle->target_charge - vehicle->current_charge;
    }
    return 1;
}

// car plugged into the charger, NULL if free
static const Vehicle* attached_vehicle(const Station* station, const Charger* charger) {
    if (charger_is_free(charger)) {
        return NULL;
    }
    return &station->cars[charger->vehicle_index];
}
