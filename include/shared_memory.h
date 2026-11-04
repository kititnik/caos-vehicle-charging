#ifndef SHARED_MEMORY_H
#define SHARED_MEMORY_H

#include <stddef.h>

#include "error_code.h"

#define SHARED_MEMORY_NAME_SIZE 64

// posix shared memory segment
typedef struct SharedMemory {
    void* data;
    size_t size;
    char name[SHARED_MEMORY_NAME_SIZE];
} SharedMemory;

// create new zero filled segment
ErrorCode shared_memory_create(SharedMemory* memory, const char* name, size_t size);

// unmap in this process
void shared_memory_close(SharedMemory* memory);
// unmap and remove the name
void shared_memory_destroy(SharedMemory* memory);

#endif //SHARED_MEMORY_H
