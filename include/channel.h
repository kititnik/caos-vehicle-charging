#ifndef CHANNEL_H
#define CHANNEL_H

#include <stddef.h>

#include "error_code.h"

#define CHANNEL_NAME_SIZE 128

typedef struct Channel {
    int fd;
    char name[CHANNEL_NAME_SIZE];
} Channel;

// create "/tmp/cvc_<run>_<base>_<index>_<direction>"
ErrorCode channel_create(Channel* channel, const char* run, const char* base, int index, const char* direction);
// blocks until the other side opens too
ErrorCode channel_open(Channel* channel, const char* run, const char* base, int index, const char* direction,
                       int flags);

ErrorCode channel_write(Channel* channel, const void* data, size_t size);
ErrorCode channel_read(Channel* channel, void* data, size_t size);

void channel_close(Channel* channel);
void channel_destroy(Channel* channel);

#endif //CHANNEL_H
