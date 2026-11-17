#ifndef REMOTE_H
#define REMOTE_H

#include "sim_context.h"

// server side of a participant that lives in a client
typedef struct Remote {
    int fd;
    // in cars or chargers array
    int index;
    // shared by all, set by the first broken exchange
    int* failed;
} Remote;

void remote_vehicle_on_tick(void* self, SimContext* sim);
void remote_charger_on_tick(void* self, SimContext* sim);

#endif //REMOTE_H
