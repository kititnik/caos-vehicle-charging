#ifndef MESSAGE_H
#define MESSAGE_H

#include "vehicle.h"
#include "charger.h"
#include "tick_order.h"

typedef enum {
    MSG_HELLO,
    MSG_WELCOME,
    MSG_TICK,
    MSG_ACTION,
    MSG_RESULT,
    MSG_END
} MessageType;

// always sent whole, fields not needed by the type are zero
typedef struct Message {
    MessageType type;
    TickOrder role;
    int index;
    int now;
    double tick_duration;
    VehicleAction action;
    Vehicle vehicle;
    Charger charger;
} Message;

#endif //MESSAGE_H
