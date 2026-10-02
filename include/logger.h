#ifndef LOGGER_H
#define LOGGER_H

#include "error_code.h"

typedef struct Logger {
    int console_fd;
    int file_fd;
    const int* now;
} Logger;

ErrorCode logger_init(Logger* logger, const int* now, const char* file_path);
void logger_close(Logger* logger);
void logger_write(const Logger* logger, const char* format, ...);

#endif //LOGGER_H
