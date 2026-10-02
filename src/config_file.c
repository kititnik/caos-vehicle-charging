#include "config_file.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARRAY_SIZE(array) ((int) (sizeof(array) / sizeof((array)[0])))

#define TOKEN_DELIMITERS " \n"

typedef ErrorCode (*KeyHandler)(Config* config, char** save);

typedef struct KeyParser {
    const char* key;
    KeyHandler handler;
} KeyParser;

typedef struct ConnectorName {
    const char* name;
    ConnectorType type;
} ConnectorName;

static ErrorCode parse_line(Config* config, char* line);
static int is_complete(const Config* config);
static char* next_token(char** save);
static int parse_double(const char* token, double* value);
static int parse_int(const char* token, int* value);
static int find_name(const char* const* names, int count, const char* name);
static int parse_connector(const char* name);
static unsigned int parse_connectors(char* list);

static ErrorCode parse_charger(Config* config, char** save);
static ErrorCode parse_car(Config* config, char** save);
static ErrorCode parse_power_limit(Config* config, char** save);
static ErrorCode parse_max_wait(Config* config, char** save);
static ErrorCode parse_tick(Config* config, char** save);
static ErrorCode parse_display_delay(Config* config, char** save);
static ErrorCode parse_distribution(Config* config, char** save);
static ErrorCode parse_selection(Config* config, char** save);
static ErrorCode parse_mode(Config* config, char** save);
static ErrorCode parse_log(Config* config, char** save);

static const char* const distribution_names[] = {"uniform", "priority", "adaptive"};
static const char* const selection_names[] = {"first_fit", "max_power", "min_power"};

static const ConnectorName connector_names[] = {
    {"TYPE1", TYPE_1},
    {"TYPE2", TYPE_2},
    {"CCS_COMBO", CCS_COMBO},
    {"CHADEMO", CHADEMO},
    {"TESLA_NACS", TESLA_NACS}
};

static const KeyParser key_parsers[] = {
    {"charger", parse_charger},
    {"car", parse_car},
    {"power_limit", parse_power_limit},
    {"max_wait", parse_max_wait},
    {"tick", parse_tick},
    {"display_delay", parse_display_delay},
    {"distribution", parse_distribution},
    {"selection", parse_selection},
    {"mode", parse_mode},
    {"log", parse_log}
};

ErrorCode config_file_load(Config* config, const char* path, int* error_line) {
    if (config == NULL || path == NULL) {
        return ERR_NULL_ARGUMENT;
    }
    *config = (Config){0};
    if (error_line != NULL) {
        *error_line = 0;
    }

    FILE* file = fopen(path, "r");
    if (file == NULL) {
        return ERR_FILE_OPEN;
    }

    char* line = NULL;
    size_t line_capacity = 0;
    int line_number = 0;
    ErrorCode error = ERR_OK;
    while (error == ERR_OK && getline(&line, &line_capacity, file) != -1) {
        line_number++;
        error = parse_line(config, line);
    }
    if (error == ERR_OK && ferror(file)) {
        error = ERR_FILE_READ;
    }
    free(line);
    fclose(file);

    if (error == ERR_OK && !is_complete(config)) {
        error = ERR_CONFIG_MISSING;
        line_number = 0;
    }
    if (error != ERR_OK) {
        config_destroy(config);
        if (error_line != NULL) {
            *error_line = line_number;
        }
    }
    return error;
}

static ErrorCode parse_line(Config* config, char* line) {
    char* comment = strchr(line, '#');
    if (comment != NULL) {
        *comment = '\0';
    }

    char* save = NULL;
    char* key = strtok_r(line, TOKEN_DELIMITERS, &save);
    if (key == NULL) {
        return ERR_OK;
    }

    for (int i = 0; i < ARRAY_SIZE(key_parsers); i++) {
        if (strcmp(key, key_parsers[i].key) == 0) {
            return key_parsers[i].handler(config, &save);
        }
    }
    return ERR_CONFIG_SYNTAX;
}

static int is_complete(const Config* config) {
    return config->chargers_count > 0 && config->cars_count > 0 && config->power_limit > 0 &&
           config->tick_duration > 0 && config->max_wait_time >= 0 && config->display_delay_ms >= 0 &&
           (config->mode == MODE_UNLIMITED || config->period_time > 0);
}

static char* next_token(char** save) {
    return strtok_r(NULL, TOKEN_DELIMITERS, save);
}

static int parse_double(const char* token, double* value) {
    if (token == NULL) {
        return 0;
    }
    char* end;
    double result = strtod(token, &end);
    if (end == token || *end != '\0' || !isfinite(result)) {
        return 0;
    }
    *value = result;
    return 1;
}

static int parse_int(const char* token, int* value) {
    if (token == NULL) {
        return 0;
    }
    char* end;
    long result = strtol(token, &end, 10);
    if (end == token || *end != '\0' || result < INT_MIN || result > INT_MAX) {
        return 0;
    }
    *value = (int) result;
    return 1;
}

static int find_name(const char* const* names, int count, const char* name) {
    if (name == NULL) {
        return -1;
    }
    for (int i = 0; i < count; i++) {
        if (strcmp(names[i], name) == 0) {
            return i;
        }
    }
    return -1;
}

static int parse_connector(const char* name) {
    if (name == NULL) {
        return 0;
    }
    for (int i = 0; i < ARRAY_SIZE(connector_names); i++) {
        if (strcmp(name, connector_names[i].name) == 0) {
            return (int) connector_names[i].type;
        }
    }
    return 0;
}

static unsigned int parse_connectors(char* list) {
    unsigned int mask = 0;
    char* save = NULL;
    for (char* name = strtok_r(list, ",", &save); name != NULL; name = strtok_r(NULL, ",", &save)) {
        int type = parse_connector(name);
        if (type == 0) {
            return 0;
        }
        mask |= (unsigned int) type;
    }
    return mask;
}

static ErrorCode parse_charger(Config* config, char** save) {
    char* name = next_token(save);
    double max_power;
    if (name == NULL || !parse_double(next_token(save), &max_power)) {
        return ERR_CONFIG_SYNTAX;
    }

    int type = parse_connector(name);
    if (type == 0 || max_power <= 0) {
        return ERR_CONFIG_VALUE;
    }

    Charger* chargers = realloc(config->chargers, (size_t)(config->chargers_count + 1) * sizeof(Charger));
    if (chargers == NULL) {
        return ERR_OUT_OF_MEMORY;
    }
    config->chargers = chargers;
    Charger* charger = &chargers[config->chargers_count];
    charger->id = config->chargers_count + 1;
    charger->type = (ConnectorType) type;
    charger->max_power = max_power;
    charger->allocated_power = 0;
    charger->vehicle = NULL;
    config->chargers_count++;
    return ERR_OK;
}

static ErrorCode parse_car(Config* config, char** save) {
    char* names = next_token(save);
    double capacity, start_percent, target_percent, max_power;
    int arrival_time;
    if (names == NULL ||
        !parse_double(next_token(save), &capacity) ||
        !parse_double(next_token(save), &start_percent) ||
        !parse_double(next_token(save), &target_percent) ||
        !parse_double(next_token(save), &max_power) ||
        !parse_int(next_token(save), &arrival_time)) {
        return ERR_CONFIG_SYNTAX;
    }

    unsigned int connectors = parse_connectors(names);
    if (connectors == 0 || capacity <= 0 || start_percent < 0 || start_percent > target_percent ||
        target_percent > 100 || max_power <= 0 || arrival_time < 0) {
        return ERR_CONFIG_VALUE;
    }

    Vehicle* cars = realloc(config->cars, (size_t) (config->cars_count + 1) * sizeof(Vehicle));
    if (cars == NULL) {
        return ERR_OUT_OF_MEMORY;
    }
    config->cars = cars;
    Vehicle* car = &cars[config->cars_count];
    car->id = config->cars_count + 1;
    car->car_state = CAR_PENDING;
    car->current_charge = capacity * start_percent / 100.0;
    car->capacity = capacity;
    car->target_charge = capacity * target_percent / 100.0;
    car->max_power = max_power;
    car->connectors = connectors;
    car->arrival_time = arrival_time;
    car->deadline_time = 0;
    car->charger = NULL;
    config->cars_count++;
    return ERR_OK;
}

static ErrorCode parse_power_limit(Config* config, char** save) {
    return parse_double(next_token(save), &config->power_limit) ? ERR_OK : ERR_CONFIG_SYNTAX;
}

static ErrorCode parse_max_wait(Config* config, char** save) {
    return parse_int(next_token(save), &config->max_wait_time) ? ERR_OK : ERR_CONFIG_SYNTAX;
}

static ErrorCode parse_tick(Config* config, char** save) {
    return parse_double(next_token(save), &config->tick_duration) ? ERR_OK : ERR_CONFIG_SYNTAX;
}

static ErrorCode parse_display_delay(Config* config, char** save) {
    return parse_int(next_token(save), &config->display_delay_ms) ? ERR_OK : ERR_CONFIG_SYNTAX;
}

static ErrorCode parse_distribution(Config* config, char** save) {
    int index = find_name(distribution_names, ARRAY_SIZE(distribution_names), next_token(save));
    if (index < 0) {
        return ERR_CONFIG_SYNTAX;
    }
    config->distribution_strategy = (DistributionStrategy) index;
    return ERR_OK;
}

static ErrorCode parse_selection(Config* config, char** save) {
    int index = find_name(selection_names, ARRAY_SIZE(selection_names), next_token(save));
    if (index < 0) {
        return ERR_CONFIG_SYNTAX;
    }
    config->selection_strategy = (SelectionStrategy) index;
    return ERR_OK;
}

static ErrorCode parse_mode(Config* config, char** save) {
    char* name = next_token(save);
    if (name == NULL) {
        return ERR_CONFIG_SYNTAX;
    }
    if (strcmp(name, "unlimited") == 0) {
        config->mode = MODE_UNLIMITED;
        return ERR_OK;
    }
    if (strcmp(name, "limited") == 0 && parse_int(next_token(save), &config->period_time)) {
        config->mode = MODE_LIMITED;
        return ERR_OK;
    }
    return ERR_CONFIG_SYNTAX;
}

static ErrorCode parse_log(Config* config, char** save) {
    char* path = next_token(save);
    if (path == NULL) {
        return ERR_CONFIG_SYNTAX;
    }
    free(config->log_path);
    config->log_path = strdup(path);
    return config->log_path != NULL ? ERR_OK : ERR_OUT_OF_MEMORY;
}
