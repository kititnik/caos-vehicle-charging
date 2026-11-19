#ifndef REMOTE_H
#define REMOTE_H

#include "sim_context.h"

// server side of a participant that lives in a client
typedef struct Remote {
    // -1 while the client is gone
    int fd;
    // in cars or chargers array
    int index;
} Remote;

void remote_vehicle_on_tick(void* self, SimContext* sim);
void remote_charger_on_tick(void* self, SimContext* sim);

#endif //REMOTE_H
