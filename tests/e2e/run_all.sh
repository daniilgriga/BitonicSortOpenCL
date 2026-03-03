#!/bin/bash
# Run all E2E tests for bitonic_sort.

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

if [ $# -eq 0 ]; then
    echo -e "${RED}ERROR: Please provide path to binary${NC}"
    echo "Usage: $0 <path_to_binary>"
    echo "Example: $0 ./build/bitonic_sort"
    exit 1
fi

BINARY="$1"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PASSED=0
FAILED=0

if [ ! -f "$BINARY" ]; then
    echo -e "${RED}ERROR: $BINARY not found!${NC}"
    exit 1
fi

echo "Running end-to-end tests for Bitonic Sort"
echo "========================================"

for dat_file in "$SCRIPT_DIR"/*.dat; do
    test_id="$(basename "$dat_file" .dat)"
    ans_file="$SCRIPT_DIR/${test_id}.ans"

    if [ ! -f "$ans_file" ]; then
        echo -e "${YELLOW}WARNING: $ans_file not found, skipping...${NC}"
        continue
    fi

    stderr_file=$(mktemp)
    actual_output=$("$BINARY" < "$dat_file" 2>"$stderr_file")
    exit_code=$?

    if [ $exit_code -ne 0 ]; then
        echo -e "Test $test_id: ${RED}[FAILED]${NC} (exit code $exit_code)"
        echo "  stderr:"
        sed 's/^/    /' "$stderr_file"
        rm -f "$stderr_file"
        ((FAILED++))
        continue
    fi

    rm -f "$stderr_file"
    expected_output=$(cat "$ans_file")

    if [ "$actual_output" == "$expected_output" ]; then
        echo -e "Test $test_id: ${GREEN}[PASSED]${NC}"
        ((PASSED++))
    else
        echo -e "Test $test_id: ${RED}[FAILED]${NC}"
        echo "  Expected: $expected_output"
        echo "  Got:      $actual_output"
        ((FAILED++))
    fi
done

echo "========================================"
echo "Results: $PASSED passed, $FAILED failed"

if [ $FAILED -gt 0 ]; then
    exit 1
else
    echo -e "${GREEN}All tests passed!${NC}"
    exit 0
fi
