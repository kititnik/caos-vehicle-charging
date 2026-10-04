#!/bin/bash

BIN="build/caos_vehicle_charging"

mkdir -p tests/out
passed=0
total=0

for config in tests/configs/*.cfg; do
    name=$(basename "$config" .cfg)
    output="tests/out/$name.txt"
    "$BIN" --config "$config" > "$output" 2>&1
    code=$?
    total=$((total + 1))
    ok=1

    # проверка кода возврата
    expected_code=$(grep "# expect-exit: " "$config" | cut -c16-)
    if [ "$code" != "$expected_code" ]; then
        echo "FAIL $name: ожидался код $expected_code, получен $code"
        ok=0
    fi

    # проверка строк в выводе
    while read -r line; do
        # специальный синтаксис [] bash для сравнения с шаблоном
        if [[ "$line" != "# expect: "* ]]; then
            continue
        fi
        expected=$(echo "$line" | cut -c11-)
        if ! grep -qF "$expected" "$output"; then
            echo "FAIL $name: не найдено \"$expected\""
            ok=0
        fi
    done < "$config"

    if [ $ok -eq 1 ]; then
        echo "PASS $name"
        passed=$((passed + 1))
    fi
done

echo "прошло $passed из $total"
