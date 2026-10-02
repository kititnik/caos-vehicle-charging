#ifndef CONFIG_H
#define CONFIG_H

#include "charger.h"
#include "vehicle.h"
#include "distribution_strategy.h"
#include "selection_strategy.h"
#include "run_mode.h"

typedef struct Config {
    Charger* chargers;
    int chargers_count;
    double power_limit;

    Vehicle* cars;
    int cars_count;
    int max_wait_time;

    DistributionStrategy distribution_strategy;
    SelectionStrategy selection_strategy;

    double tick_duration;
    int display_delay_ms;

    RunMode mode;
    int period_time;

    char* log_path;
} Config;

void config_destroy(Config* config);

#endif //CONFIG_H
