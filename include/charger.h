#ifndef CHARGER_H
#define CHARGER_H

#include "connector.h"
#include "error_code.h"

struct SimContext;
struct Vehicle;

typedef struct Charger {
    int id;
    ConnectorType type;

    double max_power;
    double allocated_power;

    // index in cars array, -1 if free
    int vehicle_index;
} Charger;

void charger_on_tick(void* self, struct SimContext* sim);

int charger_is_free(const Charger* charger);
int charger_is_compatible(const Charger* charger, const struct Vehicle* vehicle);

ErrorCode charger_attach(Charger* charger, struct Vehicle* vehicle);
void charger_detach(Charger* charger);

double charger_get_max_accepted_power(const Charger* charger, const struct Vehicle* vehicle);
double charger_accept_power(Charger* charger, const struct Vehicle* vehicle, double power);
double charger_get_power(const Charger* charger);

#endif //CHARGER_H
