#!/bin/bash
#
# run.sh - Integration SQL test runner
#
# Runs each SQL file through the client, compares output with expected.
# Test cluster data stored in /tmp/scroller/.
#

set -euo pipefail

# === Configuration ===
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
INTEGRATION_DIR="$SCRIPT_DIR"
SQL_DIR="$INTEGRATION_DIR/sql"
EXPECTED_DIR="$INTEGRATION_DIR/expected"
RESULTS_DIR="$INTEGRATION_DIR/results"

# Test cluster location
TEST_BASE="/tmp/scroller"
TEST_DIR="$TEST_BASE/sql_test_$$"

# Binaries
BIN_BASE="$PROJECT_ROOT/build/debug/bin"
SCR_INIT="$BIN_BASE/scr_init"
SCROLLER="$BIN_BASE/scroller"
SCRC="$BIN_BASE/scrc"

# Server
SERVER_PORT=8081
SERVER_PID=""

# Test user/catalog/schema
TEST_USER="test_user"
TEST_CATALOG="test_cat"
TEST_SCHEMA="test_sch"

# === Colors ===
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# === Cleanup ===
cleanup() {
    if [ -n "$SERVER_PID" ]; then
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
    rm -rf "$TEST_DIR"
}

trap cleanup EXIT INT TERM

# === Check binaries ===
check_binaries() {
    local missing=0
    for bin in "$SCR_INIT" "$SCROLLER" "$SCRC"; do
        if [ ! -x "$bin" ]; then
            echo -e "${RED}ERROR: binary not found: $bin${NC}" >&2
            missing=1
        fi
    done
    if [ $missing -eq 1 ]; then
        echo "Build the project first: make" >&2
        exit 1
    fi
}

# === Setup test cluster ===
setup_cluster() {
    echo "Setting up test cluster in $TEST_DIR"

    mkdir -p "$TEST_BASE"
    rm -rf "$TEST_DIR"

    # scr_init creates the directory itself
    "$SCR_INIT" "$TEST_DIR" > /dev/null 2>&1 || {
        echo -e "${RED}ERROR: scr_init failed${NC}" >&2
        exit 1
    }
}

# === Start server ===
start_server() {
    echo "Starting server on port $SERVER_PORT"

    "$SCROLLER" "$TEST_DIR" > /dev/null 2>&1 &
    SERVER_PID=$!

    # Wait for server to start
    local retries=10
    while [ $retries -gt 0 ]; do
        if kill -0 "$SERVER_PID" 2>/dev/null; then
            if nc -z localhost "$SERVER_PORT" 2>/dev/null; then
                echo "Server started (PID $SERVER_PID)"
                return 0
            fi
        fi
        sleep 0.2
        retries=$((retries - 1))
    done

    echo -e "${RED}ERROR: server failed to start${NC}" >&2
    exit 1
}

# === Run single test ===
run_test() {
    local sql_file="$1"
    local name
    name=$(basename "$sql_file" .sql)

    local result_file="$RESULTS_DIR/$name.out"
    local expected_file="$EXPECTED_DIR/$name.out"

    echo -n "  $name ... "

    # Run SQL through client
    if ! "$SCRC" -h localhost -p "$SERVER_PORT" \
                 -u "$TEST_USER" -c "$TEST_CATALOG" \
                 -f "$sql_file" > "$result_file" 2>&1; then
        echo -e "${RED}FAILED (client error)${NC}"
        return 1
    fi

    # Check expected file
    if [ ! -f "$expected_file" ]; then
        echo -e "${YELLOW}NO EXPECTED${NC}"
        echo "    Creating expected/$name.out from result"
        cp "$result_file" "$expected_file"
        return 0
    fi

    # Compare
    if diff -q "$expected_file" "$result_file" > /dev/null 2>&1; then
        echo -e "${GREEN}PASSED${NC}"
        return 0
    else
        echo -e "${RED}FAILED${NC}"
        echo "    Diff (expected vs actual):"
        diff "$expected_file" "$result_file" | head -20 | sed 's/^/    /'
        return 1
    fi
}

# === Main ===
main() {
    echo "=== SQL Integration Tests ==="
    echo ""

    check_binaries

    # Create results dir
    mkdir -p "$RESULTS_DIR"
    rm -f "$RESULTS_DIR"/*.out

    setup_cluster
    start_server

    echo ""
    echo "Running tests..."

    local passed=0
    local failed=0
    local skipped=0

    # Run all SQL files in numeric order
    mapfile -t sql_files < <(printf '%s\n' "$SQL_DIR"/*.sql | sort -V)

    for sql_file in "$SQL_DIR"/*.sql; do
        if [ ! -f "$sql_file" ]; then
            continue
        fi

        if run_test "$sql_file"; then
            passed=$((passed + 1))
        else
            failed=$((failed + 1))
        fi
    done

    echo ""
    echo "=== Results ==="
    echo -e "  ${GREEN}Passed:  $passed${NC}"
    if [ $failed -gt 0 ]; then
        echo -e "  ${RED}Failed:  $failed${NC}"
    else
        echo -e "  Failed:  $failed"
    fi

    echo ""

    if [ $failed -gt 0 ]; then
        echo -e "${RED}Some tests failed!${NC}"
        echo "Results: $RESULTS_DIR"
        exit 1
    fi

    echo -e "${GREEN}All SQL tests passed!${NC}"
}

main "$@"

