#!/bin/bash

# run from project root
BIN="build/caos_vehicle_charging"

mkdir -p tests/out
passed=0
total=0

for config in tests/configs/*.cfg; do
    name=$(basename "$config" .cfg)
    output="tests/out/$name.txt"
    # stdout and stderr both to the file
    "$BIN" --config "$config" > "$output" 2>&1
    code=$?
    total=$((total + 1))
    ok=1

    # cut the '# expect-exit: ', get prefix, compare with actual
    expected_code=$(grep "# expect-exit: " "$config" | cut -c16-)
    if [ "$code" != "$expected_code" ]; then
        echo "FAIL $name: ожидался код $expected_code, получен $code"
        ok=0
    fi

    # check all expected keys
    while read -r line; do
        # special syntax [] in bash to compare with template
        if [[ "$line" != "# expect: "* ]]; then
            continue
        fi
        #drops '# expect: '
        expected=$(echo "$line" | cut -c11-)
        # -q quiet, -F plain text not regex
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
