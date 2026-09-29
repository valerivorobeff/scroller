#!/bin/bash
#
# run.sh - Functional test runner
#
# Runs each test case, compares output with expected.
#

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
CASES_DIR="$SCRIPT_DIR/cases"
EXPECTED_DIR="$SCRIPT_DIR/expected"
RESULTS_DIR="$SCRIPT_DIR/results"

BIN_DIR="$PROJECT_ROOT/build/debug/bin"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

export BIN_DIR
export PROJECT_ROOT

mkdir -p "$RESULTS_DIR" "$EXPECTED_DIR"

passed=0
failed=0

echo "=== Functional tests ==="
echo ""

for case_file in "$CASES_DIR"/*.sh; do
    [ -f "$case_file" ] || continue

    name=$(basename "$case_file" .sh)
    result_file="$RESULTS_DIR/$name.out"
    expected_file="$EXPECTED_DIR/$name.out"

    echo -n "  $name ... "

    # Run test case (allow any exit code)
    set +e
    bash "$case_file" > "$result_file" 2>&1
    exit_code=$?
    set -e

    # Append exit code for comparison
    echo "exit: $exit_code" >> "$result_file"

    # Check expected
    if [ ! -f "$expected_file" ]; then
        echo -e "${YELLOW}NO EXPECTED${NC}"
        cp "$result_file" "$expected_file"
        passed=$((passed + 1))
        continue
    fi

    # Compare
    if diff -q "$expected_file" "$result_file" > /dev/null 2>&1; then
        echo -e "${GREEN}PASSED${NC}"
        passed=$((passed + 1))
    else
        echo -e "${RED}FAILED${NC}"
        diff "$expected_file" "$result_file" | head -10 | sed 's/^/      /'
        failed=$((failed + 1))
    fi
done

echo ""
echo "=== Results ==="
echo -e "  ${GREEN}Passed: $passed${NC}"
if [ $failed -gt 0 ]; then
    echo -e "  ${RED}Failed: $failed${NC}"
    exit 1
fi
echo "  Failed: $failed"
echo ""
echo -e "${GREEN}All functional tests passed!${NC}"

