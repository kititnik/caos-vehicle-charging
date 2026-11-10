#!/bin/bash

# program 3: coordinator and all participants from one terminal
# usage: ./run_independent.sh --config <file>

CONFIG="$2"

# ctrl+c reaches every process by itself, script waits for them
trap '' INT

cars=$(grep -c '^car ' "$CONFIG")
chargers=$(grep -c '^charger ' "$CONFIG")

build/cvc_coordinator --config "$CONFIG" &
coordinator=$!

for ((i = 0; i < cars; i++)); do
    build/cvc_participant --role vehicle --index $i &
done
build/cvc_participant --role station &
for ((i = 0; i < chargers; i++)); do
    build/cvc_participant --role charger --index $i &
done

wait $coordinator
code=$?

kill -TERM $(jobs -p) 2>/dev/null
wait 2>/dev/null
exit $code
