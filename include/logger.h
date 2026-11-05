#ifndef LOGGER_H
#define LOGGER_H

#include "error_code.h"
#include "sync.h"

typedef struct Logger {
    int console_fd;
    int file_fd;
    // points at now in shared state for timestamps
    const int* now;
    Sync lock;
} Logger;

ErrorCode logger_init(Logger* logger, const int* now, const char* file_path, const char* lock_name);
void logger_close(Logger* logger);
void logger_write(Logger* logger, const char* format, ...);

#endif //LOGGER_H
