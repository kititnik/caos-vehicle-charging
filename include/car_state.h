#ifndef CAR_STATE_H
#define CAR_STATE_H

typedef enum {
    CAR_PENDING,
    CAR_QUEUED,
    CAR_CHARGING,
    CAR_DONE,
    CAR_TIMED_OUT,
    CAR_INCOMPATIBLE,
    CAR_STATE_COUNT
} CarState;

#endif //CAR_STATE_H
