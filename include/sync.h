#ifndef SYNC_H
#define SYNC_H

#include <semaphore.h>

#include "error_code.h"

#define SYNC_NAME_SIZE 64

// posix semaphore
typedef struct Sync {
    sem_t* handle;
    char name[SYNC_NAME_SIZE];
} Sync;

// create "/cvc_<run>_<base>_<index>", another with the same name removes first
ErrorCode sync_create(Sync* sem, const char* run, const char* base, int index, unsigned int initial);
ErrorCode sync_open(Sync* sem, const char* run, const char* base, int index);

// returns ERR_INTERRUPTED on a signal
ErrorCode sync_wait(Sync* sem);
// same as wait but retries on a signal
ErrorCode sync_lock(Sync* sem);
ErrorCode sync_post(Sync* sem);

// close the handle in this process
void sync_close(Sync* sem);
// close and remove the name
void sync_destroy(Sync* sem);


#endif //SYNC_H
