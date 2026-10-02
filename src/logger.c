#include "logger.h"

#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

#define MESSAGE_SIZE 256

ErrorCode logger_init(Logger* logger, const int* now, const char* file_path) {
    if (logger == NULL) {
        return ERR_NULL_ARGUMENT;
    }
    logger->console_fd = STDOUT_FILENO;
    logger->file_fd = -1;
    logger->now = now;

    if (file_path != NULL) {
        logger->file_fd = open(file_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (logger->file_fd < 0) {
            return ERR_FILE_OPEN;
        }
    }
    return ERR_OK;
}

void logger_close(Logger* logger) {
    if (logger == NULL) {
        return;
    }
    if (logger->file_fd >= 0) {
        close(logger->file_fd);
    }
    logger->file_fd = -1;
}

void logger_write(const Logger* logger, const char* format, ...) {
    if (logger == NULL || format == NULL) {
        return;
    }

    char message[MESSAGE_SIZE];
    int length = 0;
    if (logger->now != NULL) {
        length = snprintf(message, sizeof(message), "[t=%d] ", *logger->now);
    }

    va_list args;
    va_start(args, format);
    length += vsnprintf(message + length, sizeof(message) - (size_t)length, format, args);
    va_end(args);

    if (length > MESSAGE_SIZE-1) {
        length = MESSAGE_SIZE-1;
    }
    message[length] = '\n';

    write(logger->console_fd, message, (size_t)length + 1);
    if (logger->file_fd >= 0) {
        write(logger->file_fd, message, (size_t)length + 1);
    }
}
