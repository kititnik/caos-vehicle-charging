#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "net.h"

#define BUFFER_SIZE 1024

int main(int argc, char** argv) {
    const char* address = NULL;
    const char* port = NULL;

    int valid = argc % 2 == 1;
    for (int i = 1; valid && i < argc; i += 2) {
        if (strcmp(argv[i], "--address") == 0) {
            address = argv[i + 1];
        } else if (strcmp(argv[i], "--port") == 0) {
            port = argv[i + 1];
        } else {
            valid = 0;
        }
    }
    if (!valid || address == NULL || port == NULL) {
        fprintf(stderr, "Usage: %s --address <ip> --port <port>\n", argv[0]);
        return 1;
    }
    signal(SIGPIPE, SIG_IGN);

    int fd;
    if (net_connect(address, port, &fd) != ERR_OK) {
        fprintf(stderr, "cannot connect to %s:%s\n", address, port);
        return 1;
    }
    Message message;
    memset(&message, 0, sizeof(message));
    message.type = MSG_WATCH;
    if (net_send(fd, &message) != ERR_OK) {
        fprintf(stderr, "cannot connect to %s:%s\n", address, port);
        close(fd);
        return 1;
    }

    char buffer[BUFFER_SIZE];
    while (1) {
        ssize_t done = read(fd, buffer, sizeof(buffer));
        if (done < 0 && errno == EINTR) {
            continue;
        }
        if (done <= 0) {
            break;
        }
        write(STDOUT_FILENO, buffer, (size_t)done);
    }
    printf("соединение с сервером закрыто\n");
    close(fd);
    return 0;
}
