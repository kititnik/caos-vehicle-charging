#include "config.h"

#include <stdlib.h>

void config_destroy(Config* config) {
    if (config == NULL) {
        return;
    }

    free(config->chargers);
    free(config->cars);

    *config = (Config){0};
}
