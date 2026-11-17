#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "logger.h"
#include "net.h"

static int run_vehicle(int fd, Logger* logger, int* now, const Message* welcome);
static int run_charger(int fd, Logger* logger, int* now, const Message* welcome);
static void report_vehicle(Logger* logger, const Vehicle* before, const Vehicle* after);

int main(int argc, char** argv) {
    const char* address = NULL;
    const char* port = NULL;
    TickOrder role = ORDER_STATION;
    int index = -1;

    int valid = argc % 2 == 1;
    for (int i = 1; valid && i < argc; i += 2) {
        if (strcmp(argv[i], "--address") == 0) {
            address = argv[i + 1];
        } else if (strcmp(argv[i], "--port") == 0) {
            port = argv[i + 1];
        } else if (strcmp(argv[i], "--role") == 0) {
            if (strcmp(argv[i + 1], "vehicle") == 0) {
                role = ORDER_VEHICLE;
            } else if (strcmp(argv[i + 1], "charger") == 0) {
                role = ORDER_CHARGER;
            }
        } else if (strcmp(argv[i], "--index") == 0) {
            char* end;
            index = (int)strtol(argv[i + 1], &end, 10);
            valid = end != argv[i + 1] && *end == '\0';
        } else {
            valid = 0;
        }
    }
    if (!valid || address == NULL || port == NULL || role == ORDER_STATION || index < 0) {
        fprintf(stderr, "Usage: %s --address <ip> --port <port> --role vehicle|charger --index <n>\n", argv[0]);
        return 1;
    }
    // server gone gives an error from write instead of killing us
    signal(SIGPIPE, SIG_IGN);

    int fd;
    if (net_connect(address, port, &fd) != ERR_OK) {
        fprintf(stderr, "cannot connect to %s:%s\n", address, port);
        return 1;
    }

    Message message;
    memset(&message, 0, sizeof(message));
    message.type = MSG_HELLO;
    message.role = role;
    message.index = index;
    if (net_send(fd, &message) != ERR_OK || net_receive(fd, &message) != ERR_OK || message.type != MSG_WELCOME) {
        fprintf(stderr, "server rejected the connection\n");
        close(fd);
        return 1;
    }

    // time comes from the server with every message
    int now = 0;
    Logger logger;
    logger_init(&logger, &now, NULL);
    int code = role == ORDER_VEHICLE ? run_vehicle(fd, &logger, &now, &message)
                                     : run_charger(fd, &logger, &now, &message);
    logger_close(&logger);
    close(fd);
    return code;
}

// get own state, decide, send back the action
static int run_vehicle(int fd, Logger* logger, int* now, const Message* welcome) {
    Vehicle last = welcome->vehicle;
    logger_write(logger, "машина %d: ёмкость %.1f кВт·ч, заряд %.1f, цель %.1f кВт·ч, прибытие на такте %d",
                 last.id, last.capacity, last.current_charge, last.target_charge, last.arrival_time);

    Message message;
    while (net_receive(fd, &message) == ERR_OK) {
        *now = message.now;
        report_vehicle(logger, &last, &message.vehicle);
        last = message.vehicle;
        if (message.type == MSG_END) {
            logger_write(logger, "моделирование завершено");
            return 0;
        }
        if (message.type != MSG_TICK) {
            break;
        }

        VehicleAction action = vehicle_decide(&last, *now);
        if (action == ACTION_ARRIVE) {
            logger_write(logger, "машина %d прибыла, запрашивает устройство", last.id);
        } else if (action == ACTION_LEAVE) {
            logger_write(logger, "машина %d: время ожидания вышло, уезжает", last.id);
        }
        memset(&message, 0, sizeof(message));
        message.type = MSG_ACTION;
        message.action = action;
        if (net_send(fd, &message) != ERR_OK) {
            break;
        }
    }
    logger_write(logger, "связь с сервером потеряна");
    return 1;
}

// get own state and car, charge it, send back the result
static int run_charger(int fd, Logger* logger, int* now, const Message* welcome) {
    double tick_duration = welcome->tick_duration;
    Charger last = welcome->charger;
    logger_write(logger, "устройство %d: мощность %.1f кВт", last.id, last.max_power);

    Message message;
    while (net_receive(fd, &message) == ERR_OK) {
        *now = message.now;
        if (message.type == MSG_END) {
            logger_write(logger, "моделирование завершено");
            return 0;
        }
        if (message.type != MSG_TICK) {
            break;
        }

        Charger* charger = &message.charger;
        Vehicle* vehicle = &message.vehicle;
        if (!charger_is_free(charger)) {
            if (charger->vehicle_index != last.vehicle_index) {
                logger_write(logger, "устройство %d: подключена машина %d", charger->id, vehicle->id);
            }
            logger_write(logger, "устройство %d: выдаёт %.1f кВт", charger->id, charger->allocated_power);
            if (vehicle_charge(vehicle, charger->allocated_power * tick_duration)) {
                logger_write(logger, "машина %d зарядилась, устройство %d свободно", vehicle->id, charger->id);
            } else {
                logger_write(logger, "машина %d: заряд %.1f / %.1f кВт·ч", vehicle->id,
                             vehicle->current_charge, vehicle->target_charge);
            }
        }
        last = *charger;

        message.type = MSG_RESULT;
        if (net_send(fd, &message) != ERR_OK) {
            break;
        }
    }
    logger_write(logger, "связь с сервером потеряна");
    return 1;
}

// what changed since the last message
static void report_vehicle(Logger* logger, const Vehicle* before, const Vehicle* after) {
    if (after->current_charge != before->current_charge) {
        logger_write(logger, "машина %d: заряд %.1f / %.1f кВт·ч", after->id,
                     after->current_charge, after->target_charge);
    }
    if (after->car_state == before->car_state) {
        return;
    }
    switch (after->car_state) {
        case CAR_QUEUED:
            logger_write(logger, "машина %d встала в очередь", after->id);
            break;
        case CAR_CHARGING:
            logger_write(logger, "машина %d подключена к устройству", after->id);
            break;
        case CAR_DONE:
            logger_write(logger, "машина %d зарядилась", after->id);
            break;
        case CAR_TIMED_OUT:
            logger_write(logger, "машина %d покинула очередь", after->id);
            break;
        case CAR_INCOMPATIBLE:
            logger_write(logger, "машина %d: нет совместимого разъёма, уехала", after->id);
            break;
        default:
            break;
    }
}
