#include "sync.h"

#include <errno.h>

// same module for named and unnamed sems

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
