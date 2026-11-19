#include "remote.h"

#include <string.h>
#include <unistd.h>

#include "net.h"

#define EPS 1e-9

static int exchange(Remote* remote, SimContext* sim, Message* message, MessageType answer_type, const char* name);
static void drop(Remote* remote, SimContext* sim, const char* name);

// send the car its state, get back what it decided, a gone car does nothing
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

// send the charger its state and car, get back the new charge, a gone charger does nothing
void remote_charger_on_tick(void* self, SimContext* sim) {
    Remote* remote = (Remote*)self;
    Charger* charger = &sim->station->chargers[remote->index];

    Message message;
    memset(&message, 0, sizeof(message));
    message.charger = *charger;
    if (!charger_is_free(charger)) {
        message.vehicle = sim->cars[charger->vehicle_index];
    }
    if (!exchange(remote, sim, &message, MSG_RESULT, "устройством")) {
        charger->offline = remote->fd < 0;
        return;
    }
    if (charger_is_free(charger)) {
        return;
    }

    Vehicle* vehicle = &sim->cars[charger->vehicle_index];
    double charge = message.vehicle.current_charge;
    double most = vehicle->current_charge + charger->allocated_power * sim->tick_duration + EPS;
    if (!(charge >= vehicle->current_charge && charge <= most && charge <= vehicle->target_charge)) {
        logger_write(sim->logger, "устройство %d прислало неверный заряд, отключено", charger->id);
        close(remote->fd);
        remote->fd = -1;
        charger->offline = 1;
        return;
    }
    vehicle->current_charge = charge;
    charger_complete_tick(charger, vehicle, charge == vehicle->target_charge, sim);
}

// 0 if the client is gone or the exchange broke
static int exchange(Remote* remote, SimContext* sim, Message* message, MessageType answer_type, const char* name) {
    if (remote->fd < 0) {
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
    // ctrl+c is not a lost connection, the server stops anyway
    if (error != ERR_OK && error != ERR_INTERRUPTED) {
        drop(remote, sim, name);
    }
    return error == ERR_OK;
}

static void drop(Remote* remote, SimContext* sim, const char* name) {
    logger_write(sim->logger, "связь с %s %d потеряна, ждём переподключения", name, remote->index + 1);
    close(remote->fd);
    remote->fd = -1;
}
