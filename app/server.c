#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config_file.h"
#include "net.h"
#include "remote.h"
#include "sim_context.h"
#include "stats.h"

#define PEER_SIZE 64

typedef struct Server {
    Config config;
    Logger logger;
    Station station;
    SimContext sim;
    // cars first, then chargers
    Remote* remotes;
    int remotes_count;
    int listen_fd;
    int failed;
} Server;

typedef enum {
    OPENED_NOTHING,
    OPENED_CONFIG,
    OPENED_LOGGER,
    OPENED_STATION,
    OPENED_SIM,
    OPENED_REMOTES,
    OPENED_SOCKET
} Opened;

static ErrorCode start_server(Server* server, const char* config_path, const char* address, const char* port);
static void stop_server(Server* server, Opened opened);
static ErrorCode subscribe_all(Server* server);
static ErrorCode accept_all(Server* server);
static ErrorCode accept_one(Server* server, int* joined);
static void accept_waiting(Server* server);
static void attach_watcher(Server* server, int fd, const char* peer);
static Remote* find_slot(Server* server, const Message* hello);
static void send_end(Server* server);
static const char* config_error_message(ErrorCode error);
static const char* end_reason(const Server* server);
static void on_interrupt(int signum);

static volatile sig_atomic_t interrupted = 0;

int main(int argc, char** argv) {
    const char* config_path = NULL;
    const char* address = NULL;
    const char* port = NULL;

    int valid = argc % 2 == 1;
    for (int i = 1; valid && i < argc; i += 2) {
        if (strcmp(argv[i], "--config") == 0) {
            config_path = argv[i + 1];
        } else if (strcmp(argv[i], "--address") == 0) {
            address = argv[i + 1];
        } else if (strcmp(argv[i], "--port") == 0) {
            port = argv[i + 1];
        } else {
            valid = 0;
        }
    }
    if (!valid || config_path == NULL || address == NULL || port == NULL) {
        fprintf(stderr, "Usage: %s --config <file> --address <ip> --port <port>\n", argv[0]);
        return 1;
    }

    // no SA_RESTART, so ctrl+c breaks out of accept and read
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = on_interrupt;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGINT, &action, NULL) != 0) {
        fprintf(stderr, "cannot install interrupt handler\n");
        return 1;
    }
    // a dead client gives an error from write instead of killing us
    signal(SIGPIPE, SIG_IGN);

    Server server;
    if (start_server(&server, config_path, address, port) != ERR_OK) {
        return 1;
    }

    logger_write(&server.logger, "ожидание участников на %s:%s: %d", address, port, server.remotes_count);
    ErrorCode error = accept_all(&server);
    if (error == ERR_OK) {
        logger_write(&server.logger, "все участники подключились");
    } else if (error != ERR_INTERRUPTED) {
        logger_write(&server.logger, "ошибка приёма подключений");
        server.failed = 1;
    }

    while (error == ERR_OK && !interrupted && !server.failed && !sim_is_finished(&server.sim)) {
        accept_waiting(&server);
        sim_tick(&server.sim);
        usleep(server.sim.display_delay_ms * 1000);
    }
    if (interrupted) {
        logger_write(&server.logger, "моделирование прервано пользователем");
    }
    stats_print(&server.sim, end_reason(&server));

    // clients get the final state and exit
    send_end(&server);
    int code = interrupted ? 128 + SIGINT : (server.failed ? 1 : 0);
    stop_server(&server, OPENED_SOCKET);
    return code;
}

// on failure prints why and leaves nothing behind
static ErrorCode start_server(Server* server, const char* config_path, const char* address, const char* port) {
    memset(server, 0, sizeof(*server));
    int error_line;
    ErrorCode error = config_file_load(&server->config, config_path, &error_line);
    if (error != ERR_OK) {
        if (error_line > 0) {
            fprintf(stderr, "%s:%d: %s\n", config_path, error_line, config_error_message(error));
        } else {
            fprintf(stderr, "%s: %s\n", config_path, config_error_message(error));
        }
        return error;
    }

    Opened opened = OPENED_CONFIG;
    error = logger_init(&server->logger, &server->sim.now, server->config.log_path);
    if (error != ERR_OK) {
        fprintf(stderr, "%s: cannot open log file\n", server->config.log_path);
    } else {
        opened = OPENED_LOGGER;
        error = station_init(&server->station, &server->config, &server->logger);
        if (error != ERR_OK) {
            fprintf(stderr, "cannot initialize station\n");
        }
    }
    if (error == ERR_OK) {
        opened = OPENED_STATION;
        error = sim_init(&server->sim, &server->config, &server->station, &server->logger);
        if (error != ERR_OK) {
            fprintf(stderr, "cannot initialize simulation\n");
        }
    }
    if (error == ERR_OK) {
        opened = OPENED_SIM;
        error = subscribe_all(server);
        if (error != ERR_OK) {
            fprintf(stderr, "cannot subscribe participants\n");
        }
    }
    if (error == ERR_OK) {
        opened = OPENED_REMOTES;
        error = net_listen(address, port, &server->listen_fd);
        if (error != ERR_OK) {
            fprintf(stderr, "cannot listen on %s:%s\n", address, port);
        }
    }
    if (error != ERR_OK) {
        stop_server(server, opened);
    }
    return error;
}

static void stop_server(Server* server, Opened opened) {
    if (opened >= OPENED_SOCKET) {
        close(server->listen_fd);
        if (server->logger.watcher_fd >= 0) {
            close(server->logger.watcher_fd);
        }
    }
    if (opened >= OPENED_REMOTES) {
        for (int i = 0; i < server->remotes_count; i++) {
            if (server->remotes[i].fd >= 0) {
                close(server->remotes[i].fd);
            }
        }
    }
    free(server->remotes);
    if (opened >= OPENED_SIM) {
        sim_destroy(&server->sim);
    }
    if (opened >= OPENED_STATION) {
        station_destroy(&server->station);
    }
    if (opened >= OPENED_LOGGER) {
        logger_close(&server->logger);
    }
    config_destroy(&server->config);
}

// cars and chargers are remote, the station runs here
static ErrorCode subscribe_all(Server* server) {
    int cars_count = server->config.cars_count;
    server->remotes_count = cars_count + server->config.chargers_count;
    server->remotes = malloc((size_t)server->remotes_count * sizeof(Remote));
    if (server->remotes == NULL) {
        return ERR_OUT_OF_MEMORY;
    }
    for (int i = 0; i < server->remotes_count; i++) {
        server->remotes[i].fd = -1;
        server->remotes[i].index = i < cars_count ? i : i - cars_count;
        server->remotes[i].failed = &server->failed;
    }

    ErrorCode error = ERR_OK;
    for (int i = 0; i < cars_count && error == ERR_OK; i++) {
        error = sim_subscribe(&server->sim, &server->remotes[i], remote_vehicle_on_tick, ORDER_VEHICLE);
    }
    if (error == ERR_OK) {
        error = sim_subscribe(&server->sim, &server->station, station_on_tick, ORDER_STATION);
    }
    for (int i = cars_count; i < server->remotes_count && error == ERR_OK; i++) {
        error = sim_subscribe(&server->sim, &server->remotes[i], remote_charger_on_tick, ORDER_CHARGER);
    }
    return error;
}

// until every slot is taken
static ErrorCode accept_all(Server* server) {
    int joined = 0;
    ErrorCode error = ERR_OK;
    while (error == ERR_OK && joined < server->remotes_count && !interrupted) {
        error = accept_one(server, &joined);
    }
    return interrupted ? ERR_INTERRUPTED : error;
}

// a bad client is just dropped, only a broken listening socket is an error
static ErrorCode accept_one(Server* server, int* joined) {
    int fd;
    char peer[PEER_SIZE];
    ErrorCode error = net_accept(server->listen_fd, &fd, peer, sizeof(peer));
    if (error != ERR_OK) {
        return error;
    }

    Message message;
    error = net_receive(fd, &message);
    if (error == ERR_OK && message.type == MSG_WATCH) {
        attach_watcher(server, fd, peer);
        return ERR_OK;
    }
    Remote* remote = error == ERR_OK ? find_slot(server, &message) : NULL;
    if (remote == NULL) {
        if (error != ERR_INTERRUPTED) {
            logger_write(&server->logger, "подключение %s отклонено", peer);
        }
        close(fd);
        return error == ERR_INTERRUPTED ? error : ERR_OK;
    }

    TickOrder role = message.role;
    memset(&message, 0, sizeof(message));
    message.type = MSG_WELCOME;
    message.role = role;
    message.index = remote->index;
    message.tick_duration = server->sim.tick_duration;
    if (role == ORDER_VEHICLE) {
        message.vehicle = server->config.cars[remote->index];
    } else {
        message.charger = server->config.chargers[remote->index];
    }
    error = net_send(fd, &message);
    if (error != ERR_OK) {
        close(fd);
        return error == ERR_INTERRUPTED ? error : ERR_OK;
    }

    remote->fd = fd;
    (*joined)++;
    if (role == ORDER_VEHICLE) {
        logger_write(&server->logger, "машина %d подключилась (%s)", remote->index + 1, peer);
    } else {
        logger_write(&server->logger, "устройство %d подключилось (%s)", remote->index + 1, peer);
    }
    return ERR_OK;
}

static void accept_waiting(Server* server) {
    int joined = 0;
    while (net_has_client(server->listen_fd) && accept_one(server, &joined) == ERR_OK) {
    }
}

// newer observer replaces the old one
static void attach_watcher(Server* server, int fd, const char* peer) {
    if (server->logger.watcher_fd >= 0) {
        close(server->logger.watcher_fd);
    }
    server->logger.watcher_fd = fd;
    logger_write(&server->logger, "наблюдатель подключился (%s)", peer);
}

// free slot for the role and index from hello, NULL if wrong or taken
static Remote* find_slot(Server* server, const Message* hello) {
    if (hello->type != MSG_HELLO || hello->index < 0) {
        return NULL;
    }
    int cars_count = server->config.cars_count;
    Remote* remote = NULL;
    if (hello->role == ORDER_VEHICLE && hello->index < cars_count) {
        remote = &server->remotes[hello->index];
    } else if (hello->role == ORDER_CHARGER && hello->index < server->config.chargers_count) {
        remote = &server->remotes[cars_count + hello->index];
    }
    return remote != NULL && remote->fd < 0 ? remote : NULL;
}

// errors do not matter, connections are closed right after
static void send_end(Server* server) {
    int cars_count = server->config.cars_count;
    for (int i = 0; i < server->remotes_count; i++) {
        Remote* remote = &server->remotes[i];
        if (remote->fd < 0) {
            continue;
        }
        Message message;
        memset(&message, 0, sizeof(message));
        message.type = MSG_END;
        message.index = remote->index;
        message.now = server->sim.now;
        if (i < cars_count) {
            message.vehicle = server->config.cars[remote->index];
        } else {
            message.charger = server->config.chargers[remote->index];
        }
        net_send(remote->fd, &message);
    }
}

static const char* end_reason(const Server* server) {
    if (interrupted) {
        return "прервано пользователем";
    }
    if (server->failed) {
        return "ошибка сети";
    }
    if (server->sim.mode == MODE_LIMITED && server->sim.now >= server->sim.period_time) {
        return "окончание периода";
    }
    return "все машины обслужены";
}

static void on_interrupt(int signum) {
    interrupted = 1;
}

static const char* config_error_message(ErrorCode error) {
    switch (error) {
        case ERR_FILE_OPEN:
            return "cannot open file";
        case ERR_FILE_READ:
            return "cannot read file";
        case ERR_OUT_OF_MEMORY:
            return "out of memory";
        case ERR_CONFIG_SYNTAX:
            return "syntax error";
        case ERR_CONFIG_VALUE:
            return "invalid value";
        case ERR_CONFIG_MISSING:
            return "missing required parameters";
        default:
            return "unknown error";
    }
}
