#include "sync.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

ErrorCode sync_create(Sync* sem, const char* name, unsigned int initial) {
    if (sem == NULL || name == NULL) {
        return ERR_NULL_ARGUMENT;
    }
    sem->handle = NULL;
    sem->name[0] = '\0';

    size_t len = strlen(name);
    if (len >= SYNC_NAME_SIZE) {
        return ERR_INVALID_ARGUMENT;
    }

    sem_unlink(name);
    sem_t* handle = sem_open(name, O_CREAT | O_EXCL, 0600, initial);
    if (handle == SEM_FAILED) {
        return ERR_SEMAPHORE;
    }
    sem->handle = handle;
    memcpy(sem->name, name, len + 1);
    return ERR_OK;
}

ErrorCode sync_wait(Sync* sem) {
    if (sem_wait(sem->handle) == 0) {
        return ERR_OK;
    }
    return errno == EINTR ? ERR_INTERRUPTED : ERR_SEMAPHORE;
}

ErrorCode sync_lock(Sync* sem) {
    ErrorCode error;
    do {
        error = sync_wait(sem);
    } while (error == ERR_INTERRUPTED);
    return error;
}

ErrorCode sync_post(Sync* sem) {
    return sem_post(sem->handle) == 0 ? ERR_OK : ERR_SEMAPHORE;
}

void sync_close(Sync* sem) {
    if (sem == NULL || sem->handle == NULL) {
        return;
    }
    sem_close(sem->handle);
    sem->handle = NULL;
}

void sync_destroy(Sync* sem) {
    if (sem == NULL) {
        return;
    }
    sync_close(sem);
    if (sem->name[0] != '\0') {
        sem_unlink(sem->name);
        sem->name[0] = '\0';
    }
}

ErrorCode sync_make_name(char* out, size_t size, const char* run, const char* base, int index) {
    if (out == NULL || run == NULL || base == NULL) {
        return ERR_NULL_ARGUMENT;
    }
    int length = snprintf(out, size, "/cvc_%s_%s_%d", run, base, index);
    if ((size_t)length >= size) {
        return ERR_INVALID_ARGUMENT;
    }
    return ERR_OK;
}
