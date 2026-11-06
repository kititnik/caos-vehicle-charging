#include "sync.h"

#include <stdio.h>
#include <sys/mman.h>

#include "shared_memory.h"

ErrorCode sync_create(Sync* sem, const char* run, const char* base, int index, unsigned int initial) {
    if (sem == NULL || run == NULL || base == NULL) {
        return ERR_NULL_ARGUMENT;
    }
    sem->handle = NULL;
    sem->name[0] = '\0';

    char segment[SYNC_NAME_SIZE];
    int length = snprintf(segment, sizeof(segment), "%s_%d", base, index);
    if ((size_t)length >= sizeof(segment)) {
        return ERR_INVALID_ARGUMENT;
    }

    SharedMemory memory;
    if (shared_memory_create(&memory, run, segment, sizeof(sem_t)) != ERR_OK) {
        return ERR_SEMAPHORE;
    }
    if (sem_init(memory.data, 1, initial) != 0) {
        shared_memory_destroy(&memory);
        return ERR_SEMAPHORE;
    }
    sem->handle = memory.data;
    snprintf(sem->name, sizeof(sem->name), "%s", memory.name);
    return ERR_OK;
}

// map the segment of a semaphore another process already initialized
ErrorCode sync_open(Sync* sem, const char* run, const char* base, int index) {
    if (sem == NULL || run == NULL || base == NULL) {
        return ERR_NULL_ARGUMENT;
    }
    sem->handle = NULL;
    sem->name[0] = '\0';

    char segment[SYNC_NAME_SIZE];
    int length = snprintf(segment, sizeof(segment), "%s_%d", base, index);
    if ((size_t)length >= sizeof(segment)) {
        return ERR_INVALID_ARGUMENT;
    }

    SharedMemory memory;
    ErrorCode error = shared_memory_open(&memory, run, segment);
    if (error != ERR_OK) {
        return error;
    }
    sem->handle = memory.data;
    snprintf(sem->name, sizeof(sem->name), "%s", memory.name);
    return ERR_OK;
}

void sync_close(Sync* sem) {
    if (sem == NULL || sem->handle == NULL) {
        return;
    }
    munmap(sem->handle, sizeof(sem_t));
    sem->handle = NULL;
}

void sync_destroy(Sync* sem) {
    if (sem == NULL) {
        return;
    }
    if (sem->handle != NULL) {
        sem_destroy(sem->handle);
    }
    sync_close(sem);
    if (sem->name[0] != '\0') {
        shm_unlink(sem->name);
        sem->name[0] = '\0';
    }
}
