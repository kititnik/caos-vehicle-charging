#include "stats.h"

#include "logger.h"

void stats_print(const SimContext* sim, const char* reason) {
    static const char* const car_state_names[CAR_STATE_COUNT] = {
        "не приехала", "в очереди", "заряжается", "зарядилась", "ушла по тайм-ауту", "несовместима"
    };

    int counts[CAR_STATE_COUNT] = {0};
    for (int i = 0; i < sim->cars_count; i++) {
        counts[sim->cars[i].car_state]++;
    }

    logger_write(sim->logger, "===== итоги =====");
    logger_write(sim->logger, "завершение: %s", reason);
    logger_write(sim->logger, "прошло тактов: %d (%.2f ч)", sim->now, sim->now * sim->tick_duration);
    logger_write(sim->logger, "машин всего: %d", sim->cars_count);
    logger_write(sim->logger, "  зарядились: %d", counts[CAR_DONE]);
    logger_write(sim->logger, "  ушли по тайм-ауту: %d", counts[CAR_TIMED_OUT]);
    logger_write(sim->logger, "  несовместимы: %d", counts[CAR_INCOMPATIBLE]);
    logger_write(sim->logger, "  не обслужены к концу: %d",
                 counts[CAR_PENDING] + counts[CAR_QUEUED] + counts[CAR_CHARGING]);
    for (int i = 0; i < sim->cars_count; i++) {
        const Vehicle* vehicle = &sim->cars[i];
        logger_write(sim->logger, "машина %d: %s, %.1f%% (цель %.1f%%)", vehicle->id,
                     car_state_names[vehicle->car_state],
                     vehicle->current_charge / vehicle->capacity * 100.0,
                     vehicle->target_charge / vehicle->capacity * 100.0);
    }
}
