#ifndef CHARGER_H
#define CHARGER_H

#include "connector.h"

typedef struct Charger {
    int id;
    ConnectorType type;

    double max_power;
    double allocated_power;

    struct Vehicle* vehicle;
} Charger;

#endif //CHARGER_H
