#include "shared_memory.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static ErrorCode make_name(SharedMemory* memory, char* name, const char* run, const char* base);

ErrorCode shared_memory_create(SharedMemory* memory, const char* run, const char* base, size_t size) {
    char name[SHARED_MEMORY_NAME_SIZE];
    ErrorCode error = make_name(memory, name, run, base);
    if (error != ERR_OK) {
        return error;
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
    memcpy(memory->name, name, sizeof(name));
    return ERR_OK;
}

ErrorCode shared_memory_open(SharedMemory* memory, const char* run, const char* base) {
    char name[SHARED_MEMORY_NAME_SIZE];
    ErrorCode error = make_name(memory, name, run, base);
    if (error != ERR_OK) {
        return error;
    }

    int fd = shm_open(name, O_RDWR, 0);
    if (fd < 0) {
        return errno == ENOENT ? ERR_NOT_FOUND : ERR_SHARED_MEMORY;
    }
    struct stat info;
    if (fstat(fd, &info) != 0) {
        close(fd);
        return ERR_SHARED_MEMORY;
    }
    // created but not sized yet, try again later
    if (info.st_size == 0) {
        close(fd);
        return ERR_NOT_FOUND;
    }
    void* data = mmap(NULL, (size_t)info.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (data == MAP_FAILED) {
        return ERR_SHARED_MEMORY;
    }

    memory->data = data;
    memory->size = (size_t)info.st_size;
    memcpy(memory->name, name, sizeof(name));
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

// "/cvc_<run>_<base>", also resets memory so close/destroy are safe on failure
static ErrorCode make_name(SharedMemory* memory, char* name, const char* run, const char* base) {
    if (memory == NULL || run == NULL || base == NULL) {
        return ERR_NULL_ARGUMENT;
    }
    memory->data = NULL;
    memory->size = 0;
    memory->name[0] = '\0';
    int length = snprintf(name, SHARED_MEMORY_NAME_SIZE, "/cvc_%s_%s", run, base);
    if ((size_t)length >= SHARED_MEMORY_NAME_SIZE) {
        return ERR_INVALID_ARGUMENT;
    }
    return ERR_OK;
}
