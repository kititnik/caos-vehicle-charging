#include "shared_memory.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

ErrorCode shared_memory_create(SharedMemory* memory, const char* run, const char* base, size_t size) {
    if (memory == NULL || run == NULL || base == NULL) {
        return ERR_NULL_ARGUMENT;
    }
    memory->data = NULL;
    memory->size = 0;
    memory->name[0] = '\0';

    char name[SHARED_MEMORY_NAME_SIZE];
    int length = snprintf(name, sizeof(name), "/cvc_%s_%s", run, base);
    if ((size_t)length >= sizeof(name)) {
        return ERR_INVALID_ARGUMENT;
    }

    shm_unlink(name);
    int fd = shm_open(name, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (fd < 0) {
        return ERR_SHARED_MEMORY;
    }
    if (ftruncate(fd, (off_t)size) != 0) {
        close(fd);
        shm_unlink(name);
        return ERR_SHARED_MEMORY;
    }
    void* data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (data == MAP_FAILED) {
        shm_unlink(name);
        return ERR_SHARED_MEMORY;
    }

    memory->data = data;
    memory->size = size;
    memcpy(memory->name, name, (size_t)length + 1);
    return ERR_OK;
}

void shared_memory_close(SharedMemory* memory) {
    if (memory == NULL || memory->data == NULL) {
        return;
    }
    munmap(memory->data, memory->size);
    memory->data = NULL;
    memory->size = 0;
}

void shared_memory_destroy(SharedMemory* memory) {
    if (memory == NULL) {
        return;
    }
    shared_memory_close(memory);
    if (memory->name[0] != '\0') {
        shm_unlink(memory->name);
        memory->name[0] = '\0';
    }
}
