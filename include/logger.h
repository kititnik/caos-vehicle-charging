#ifndef LOGGER_H
#define LOGGER_H

#include <pthread.h>

#include "error_code.h"

typedef struct Logger {
    int console_fd;
    int file_fd;
    // points at sim.now for timestamps
    const int* now;
    pthread_mutex_t lock;
} Logger;

ErrorCode logger_init(Logger* logger, const int* now, const char* file_path);
void logger_close(Logger* logger);
void logger_write(Logger* logger, const char* format, ...);

#endif //LOGGER_H
