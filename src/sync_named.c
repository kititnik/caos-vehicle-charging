#include "sync.h"

#include <fcntl.h>
#include <stdio.h>

ErrorCode sync_create(Sync* sem, const char* run, const char* base, int index, unsigned int initial) {
    if (sem == NULL || run == NULL || base == NULL) {
        return ERR_NULL_ARGUMENT;
    }
    sem->handle = NULL;

    int length = snprintf(sem->name, sizeof(sem->name), "/cvc_%s_%s_%d", run, base, index);
    if ((size_t)length >= sizeof(sem->name)) {
        sem->name[0] = '\0';
        return ERR_INVALID_ARGUMENT;
    }

    sem_unlink(sem->name);
    sem_t* handle = sem_open(sem->name, O_CREAT | O_EXCL, 0600, initial);
    if (handle == SEM_FAILED) {
        sem->name[0] = '\0';
        return ERR_SEMAPHORE;
    }
    sem->handle = handle;
    return ERR_OK;
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
