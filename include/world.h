#ifndef WORLD_H
#define WORLD_H

#include "shared_state.h"
#include "channel.h"
#include "error_code.h"

typedef struct World {
    SharedState* state;
    Vehicle* cars;
    Charger* chargers;
    int* queue;
} World;

ErrorCode world_create(World* world, const Config* config);
void world_destroy(World* world);

ErrorCode world_send(Channel* channel, const World* world);
ErrorCode world_receive(Channel* channel, World* world);

#endif //WORLD_H
