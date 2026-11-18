#include "net.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

static ErrorCode make_address(struct sockaddr_in* result, const char* address, const char* port);

ErrorCode net_listen(const char* address, const char* port, int* fd) {
    struct sockaddr_in result;
    ErrorCode error = make_address(&result, address, port);
    if (error != ERR_OK) {
        return error;
    }

    *fd = socket(AF_INET, SOCK_STREAM, 0);
    if (*fd < 0) {
        return ERR_NETWORK;
    }
    int yes = 1;
    setsockopt(*fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    if (bind(*fd, (struct sockaddr*)&result, sizeof(result)) != 0 || listen(*fd, SOMAXCONN) != 0) {
        close(*fd);
        *fd = -1;
        return ERR_NETWORK;
    }
    return ERR_OK;
}

ErrorCode net_accept(int listen_fd, int* fd, char* peer, size_t peer_size) {
    struct sockaddr_in client;
    socklen_t size = sizeof(client);
    *fd = accept(listen_fd, (struct sockaddr*)&client, &size);
    if (*fd < 0) {
        return errno == EINTR ? ERR_INTERRUPTED : ERR_NETWORK;
    }
    snprintf(peer, peer_size, "%s:%d", inet_ntoa(client.sin_addr), ntohs(client.sin_port));
    return ERR_OK;
}

int net_has_client(int listen_fd) {
    fd_set set;
    FD_ZERO(&set);
    FD_SET(listen_fd, &set);
    struct timeval timeout = {0, 0};
    return select(listen_fd + 1, &set, NULL, NULL, &timeout) > 0;
}

ErrorCode net_connect(const char* address, const char* port, int* fd) {
    struct sockaddr_in server;
    ErrorCode error = make_address(&server, address, port);
    if (error != ERR_OK) {
        return error;
    }

    *fd = socket(AF_INET, SOCK_STREAM, 0);
    if (*fd < 0) {
        return ERR_NETWORK;
    }
    if (connect(*fd, (struct sockaddr*)&server, sizeof(server)) != 0) {
        error = errno == EINTR ? ERR_INTERRUPTED : ERR_NETWORK;
        close(*fd);
        *fd = -1;
        return error;
    }
    return ERR_OK;
}

ErrorCode net_send(int fd, const Message* message) {
    const char* bytes = (const char*)message;
    size_t size = sizeof(*message);
    while (size > 0) {
        ssize_t done = write(fd, bytes, size);
        if (done < 0 && errno == EINTR) {
            return ERR_INTERRUPTED;
        }
        if (done <= 0) {
            return ERR_NETWORK;
        }
        bytes += done;
        size -= (size_t)done;
    }
    return ERR_OK;
}

ErrorCode net_receive(int fd, Message* message) {
    char* bytes = (char*)message;
    size_t size = sizeof(*message);
    while (size > 0) {
        ssize_t done = read(fd, bytes, size);
        if (done < 0 && errno == EINTR) {
            return ERR_INTERRUPTED;
        }
        // 0 is the other side closed
        if (done <= 0) {
            return ERR_NETWORK;
        }
        bytes += done;
        size -= (size_t)done;
    }
    return ERR_OK;
}

static ErrorCode make_address(struct sockaddr_in* result, const char* address, const char* port) {
    char* end;
    long number = strtol(port, &end, 10);
    if (end == port || *end != '\0' || number < 1 || number > 65535) {
        return ERR_INVALID_ARGUMENT;
    }
    memset(result, 0, sizeof(*result));
    result->sin_family = AF_INET;
    result->sin_port = htons((unsigned short)number);
    result->sin_addr.s_addr = inet_addr(address);
    if (result->sin_addr.s_addr == INADDR_NONE) {
        return ERR_INVALID_ARGUMENT;
    }
    return ERR_OK;
}
