#include "remote.h"

#include <string.h>

#include "net.h"

#define EPS 1e-9

static int exchange(Remote* remote, SimContext* sim, Message* message, MessageType answer_type, const char* name);

// send the car its state, get back what it decided
void remote_vehicle_on_tick(void* self, SimContext* sim) {
    Remote* remote = (Remote*)self;
    Vehicle* vehicle = &sim->cars[remote->index];

    Message message;
    memset(&message, 0, sizeof(message));
    message.vehicle = *vehicle;
    if (exchange(remote, sim, &message, MSG_ACTION, "машиной")) {
        vehicle_act(vehicle, message.action, sim);
    }
}

// send the charger its state and car, get back the new charge
void remote_charger_on_tick(void* self, SimContext* sim) {
    Remote* remote = (Remote*)self;
    Charger* charger = &sim->station->chargers[remote->index];

    Message message;
    memset(&message, 0, sizeof(message));
    message.charger = *charger;
    if (!charger_is_free(charger)) {
        message.vehicle = sim->cars[charger->vehicle_index];
    }
    if (!exchange(remote, sim, &message, MSG_RESULT, "устройством") || charger_is_free(charger)) {
        return;
    }

    Vehicle* vehicle = &sim->cars[charger->vehicle_index];
    double charge = message.vehicle.current_charge;
    double most = vehicle->current_charge + charger->allocated_power * sim->tick_duration + EPS;
    if (!(charge >= vehicle->current_charge && charge <= most && charge <= vehicle->target_charge)) {
        logger_write(sim->logger, "устройство %d прислало неверный заряд", charger->id);
        *remote->failed = 1;
        return;
    }
    vehicle->current_charge = charge;
    charger_complete_tick(charger, vehicle, charge == vehicle->target_charge, sim);
}

static int exchange(Remote* remote, SimContext* sim, Message* message, MessageType answer_type, const char* name) {
    if (*remote->failed) {
        return 0;
    }
    message->type = MSG_TICK;
    message->index = remote->index;
    message->now = sim->now;

    ErrorCode error = net_send(remote->fd, message);
    if (error == ERR_OK) {
        error = net_receive(remote->fd, message);
    }
    if (error == ERR_OK && message->type != answer_type) {
        error = ERR_NETWORK;
    }
    if (error != ERR_OK) {
        // ctrl+c is not a lost connection
        if (error != ERR_INTERRUPTED) {
            logger_write(sim->logger, "связь с %s %d потеряна", name, remote->index + 1);
        }
        *remote->failed = 1;
        return 0;
    }
    return 1;
}
