#include "channel.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

static ErrorCode make_name(Channel* channel, const char* run, const char* base, int index, const char* direction);

ErrorCode channel_create(Channel* channel, const char* run, const char* base, int index, const char* direction) {
    ErrorCode error = make_name(channel, run, base, index, direction);
    if (error != ERR_OK) {
        return error;
    }

    unlink(channel->name);
    if (mkfifo(channel->name, 0600) != 0) {
        channel->name[0] = '\0';
        return ERR_CHANNEL;
    }
    return ERR_OK;
}

ErrorCode channel_open(Channel* channel, const char* run, const char* base, int index, const char* direction,
                       int flags) {
    ErrorCode error = make_name(channel, run, base, index, direction);
    if (error != ERR_OK) {
        return error;
    }

    channel->fd = open(channel->name, flags);
    if (channel->fd < 0) {
        return errno == EINTR ? ERR_INTERRUPTED : ERR_CHANNEL;
    }
    return ERR_OK;
}

ErrorCode channel_write(Channel* channel, const void* data, size_t size) {
    const char* bytes = data;
    while (size > 0) {
        ssize_t done = write(channel->fd, bytes, size);
        if (done < 0 && errno == EINTR) {
            continue;
        }
        if (done <= 0) {
            return ERR_CHANNEL;
        }
        bytes += done;
        size -= (size_t)done;
    }
    return ERR_OK;
}

ErrorCode channel_read(Channel* channel, void* data, size_t size) {
    char* bytes = data;
    while (size > 0) {
        ssize_t done = read(channel->fd, bytes, size);
        if (done < 0 && errno == EINTR) {
            continue;
        }
        if (done <= 0) {
            return ERR_CHANNEL;
        }
        bytes += done;
        size -= (size_t)done;
    }
    return ERR_OK;
}

void channel_close(Channel* channel) {
    if (channel == NULL || channel->fd < 0) {
        return;
    }
    close(channel->fd);
    channel->fd = -1;
}

void channel_destroy(Channel* channel) {
    if (channel == NULL) {
        return;
    }
    channel_close(channel);
    if (channel->name[0] != '\0') {
        unlink(channel->name);
        channel->name[0] = '\0';
    }
}

static ErrorCode make_name(Channel* channel, const char* run, const char* base, int index, const char* direction) {
    if (channel == NULL || run == NULL || base == NULL || direction == NULL) {
        return ERR_NULL_ARGUMENT;
    }
    channel->fd = -1;
    int length = snprintf(channel->name, sizeof(channel->name), "/tmp/cvc_%s_%s_%d_%s", run, base, index, direction);
    if ((size_t)length >= sizeof(channel->name)) {
        channel->name[0] = '\0';
        return ERR_INVALID_ARGUMENT;
    }
    return ERR_OK;
}
