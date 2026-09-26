#ifndef CONNECTOR_H
#define CONNECTOR_H

typedef enum {
    TYPE_1 = 1 << 0,
    TYPE_2 = 1 << 1,
    CCS_COMBO = 1 << 2,
    CHADEMO = 1 << 3,
    TESLA_NACS = 1 << 4
} ConnectorType;

#endif //CONNECTOR_H
