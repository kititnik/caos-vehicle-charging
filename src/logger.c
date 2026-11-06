#include "logger.h"

#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

#define MESSAGE_SIZE 256

static void close_file(Logger* logger);

ErrorCode logger_init(Logger* logger, const int* now, const char* file_path, const char* run, int owner) {
    if (logger == NULL) {
        return ERR_NULL_ARGUMENT;
    }
    logger->console_fd = STDOUT_FILENO;
    logger->file_fd = -1;
    logger->now = now;
    logger->owner = owner;

    if (file_path != NULL) {
        int flags = O_WRONLY | O_CREAT | O_APPEND | (owner ? O_TRUNC : 0);
        logger->file_fd = open(file_path, flags, 0644);
        if (logger->file_fd < 0) {
            return ERR_FILE_OPEN;
        }
    }
    ErrorCode error = owner ? sync_create(&logger->lock, run, "log", 0, 1) : sync_open(&logger->lock, run, "log", 0);
    if (error != ERR_OK) {
        close_file(logger);
        return error;
    }
    return ERR_OK;
}

void logger_close(Logger* logger) {
    if (logger == NULL) {
        return;
    }
    close_file(logger);
    if (logger->owner) {
        sync_destroy(&logger->lock);
    } else {
        sync_close(&logger->lock);
    }
}

static void close_file(Logger* logger) {
    if (logger->file_fd >= 0) {
        close(logger->file_fd);
    }
    logger->file_fd = -1;
}

// like printf, adds [t=N] and newline, writes to both outputs
void logger_write(Logger* logger, const char* format, ...) {
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

    // snprintf returns full length even when it cuts, so clamp
    if (length > MESSAGE_SIZE-1) {
        length = MESSAGE_SIZE-1;
    }
    message[length] = '\n';

    sync_lock(&logger->lock);
    write(logger->console_fd, message, (size_t)length + 1);
    if (logger->file_fd >= 0) {
        write(logger->file_fd, message, (size_t)length + 1);
    }
    sync_post(&logger->lock);
}
