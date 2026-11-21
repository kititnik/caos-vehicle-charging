#!/bin/bash

# run from project root, build directory is the first argument
BUILD="${1:-build}"
CONFIG=tests/net/station.cfg
ARGS="--address 127.0.0.1 --port 5300"
OUT=tests/out/net

mkdir -p $OUT
passed=0
total=0

check() {
    total=$((total + 1))
    if grep -qF "$2" "$OUT/$1.txt"; then
        passed=$((passed + 1))
    else
        echo "FAIL $1: не найдено \"$2\""
    fi
}

server() {
    $BUILD/cvc_server --config $CONFIG $ARGS > $OUT/$1.txt 2>&1 &
    sleep 0.3
}

clients() {
    pids=()
    for i in 0 1 2 3; do
        $BUILD/cvc_client $ARGS --role vehicle --index $i > $OUT/$1_v$i.txt &
        pids+=($!)
    done
    for i in 0 1; do
        $BUILD/cvc_client $ARGS --role charger --index $i > $OUT/$1_c$i.txt &
        pids+=($!)
    done
}

echo "Группа 1: наблюдатели"
server obs
$BUILD/cvc_observer $ARGS > $OUT/obs_1.txt &
first=$!
$BUILD/cvc_observer $ARGS > $OUT/obs_2.txt &
clients obs
sleep 0.5
kill $first
wait $first 2>/dev/null
sleep 0.5
$BUILD/cvc_observer $ARGS > $OUT/obs_3.txt &
wait 2>/dev/null
check obs "наблюдатель отключился"
check obs_2 "===== итоги ====="
check obs_3 "===== итоги ====="

echo "Группа 2: переподключение"
server re
clients re
sleep 0.3
kill ${pids[2]}
wait ${pids[2]} 2>/dev/null
sleep 0.3
kill ${pids[4]}
wait ${pids[4]} 2>/dev/null
sleep 2.5
$BUILD/cvc_client $ARGS --role vehicle --index 2 > $OUT/re_v2_again.txt &
$BUILD/cvc_client $ARGS --role charger --index 0 > $OUT/re_c0_again.txt &
wait 2>/dev/null
check re "связь с машиной 3 потеряна"
check re "связь с устройством 1 потеряна"
check re "машине 3 назначено устройство 2"
check re "все машины обслужены"

echo "Группа 3: остановка сервера"
server stop
server_pid=$!
clients stop
sleep 1
kill -TERM $server_pid
wait 2>/dev/null
check stop "прервано пользователем"
for who in v0 v1 v2 v3 c0 c1; do
    check stop_$who "моделирование завершено"
done

echo "прошло $passed из $total"
