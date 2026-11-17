#!/bin/bash

# server and all clients on this machine from one terminal
# usage: ./run_tcp.sh --config <file> [port]

CONFIG="$2"
PORT="${3:-5000}"

# ctrl+c goes to the server only, clients exit when it closes connections
trap '' INT

cars=$(grep -c '^car ' "$CONFIG")
chargers=$(grep -c '^charger ' "$CONFIG")

build/cvc_server --config "$CONFIG" --address 127.0.0.1 --port "$PORT" &
server=$!

sleep 0.5

for ((i = 0; i < cars; i++)); do
    build/cvc_client --address 127.0.0.1 --port "$PORT" --role vehicle --index $i &
done
for ((i = 0; i < chargers; i++)); do
    build/cvc_client --address 127.0.0.1 --port "$PORT" --role charger --index $i &
done

wait $server
code=$?

kill -TERM $(jobs -p) 2>/dev/null
wait 2>/dev/null
exit $code
