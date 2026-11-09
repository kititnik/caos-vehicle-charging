#!/bin/bash

# program 4: coordinator and all participants from one terminal
# usage: ./run_fifo.sh --config <file>

CONFIG="$2"

trap '' INT

cars=$(grep -c '^car ' "$CONFIG")
chargers=$(grep -c '^charger ' "$CONFIG")

build/cvc_fifo_coordinator --config "$CONFIG" &
coordinator=$!

for ((i = 0; i < cars; i++)); do
    build/cvc_fifo_participant --role vehicle --index $i &
done
build/cvc_fifo_participant --role station &
for ((i = 0; i < chargers; i++)); do
    build/cvc_fifo_participant --role charger --index $i &
done

wait $coordinator
code=$?

kill -INT $(jobs -p) 2>/dev/null
wait
exit $code
