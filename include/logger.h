#ifndef LOGGER_H
#define LOGGER_H

#include "error_code.h"

#define MAX_WATCHERS 16

typedef struct Logger {
    int console_fd;
    int file_fd;
    // observer sockets or -1, not owned
    int watcher_fds[MAX_WATCHERS];
    // points at sim.now for timestamps
    const int* now;
} Logger;

ErrorCode logger_init(Logger* logger, const int* now, const char* file_path);
void logger_close(Logger* logger);
void logger_write(const Logger* logger, const char* format, ...);

#endif //LOGGER_H
