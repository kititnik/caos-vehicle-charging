#ifndef LOGGER_H
#define LOGGER_H

typedef struct Logger {
    int console_fd;
    int file_fd;
} Logger;

#endif //LOGGER_H
