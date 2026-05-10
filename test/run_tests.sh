#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."
HASTE="./haste"
PASS=0
FAIL=0

green() { printf "\033[32m%s\033[0m\n" "$1"; }
red()   { printf "\033[31m%s\033[0m\n" "$1"; }

# Lexing tests (token output)
for file in test/lexing/*.haste; do
    name="$(basename "$file" .haste)"
    expected="test/lexing/${name}.tokens.expected"
    got="test/lexing/${name}.tokens.got"
    if $HASTE --tokens "$file" > "$got" 2>&1; then
        if diff -u "$expected" "$got" > /dev/null 2>&1; then
            green "PASS: $name (tokens)"
            PASS=$((PASS + 1))
        else
            red "FAIL: $name (tokens — output mismatch)"
            diff -u "$expected" "$got" || true
            FAIL=$((FAIL + 1))
        fi
    else
        red "FAIL: $name (tokens — compiler crash)"
        FAIL=$((FAIL + 1))
    fi
done

# Integration tests (full compilation)
for file in test/integration/*.haste; do
    name="$(basename "$file" .haste)"
    expected="test/integration/${name}.expected"
    got="test/integration/${name}.got"

    if $HASTE --llvm "$file" > "$got" 2>&1; then
        if diff -u <(tail -n +3 "$expected") <(tail -n +3 "$got") > /dev/null 2>&1; then
            green "PASS: $name (full)"
            PASS=$((PASS + 1))
        else
            red "FAIL: $name (full — output mismatch)"
            diff -u <(tail -n +3 "$expected") <(tail -n +3 "$got") || true
            FAIL=$((FAIL + 1))
        fi
    else
        red "FAIL: $name (full — compiler crash)"
        FAIL=$((FAIL + 1))
    fi
done

git checkout -- main.haste 2>/dev/null || true

echo "---"
if [ $FAIL -eq 0 ]; then
    green "All tests passed!"
else
    red "$FAIL test(s) failed, $PASS passed"
fi
exit $FAIL
