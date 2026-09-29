#!/bin/bash
#
# update_expected.sh - Update expected/ from results/
#

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EXPECTED_DIR="$SCRIPT_DIR/expected"
RESULTS_DIR="$SCRIPT_DIR/results"

usage() {
    echo "Usage: $(basename "$0") [-a|--all] | [-l|--list] | <test> [<test>...]"
    exit "${1:-0}"
}

list_tests() {
    [ -d "$RESULTS_DIR" ] || { echo "Run ./run.sh first"; exit 1; }
    echo "Available tests:"
    find "$RESULTS_DIR" -name '*.out' -printf '  %f\n' | sed 's/\.out$//' | sort
    exit 0
}

update_one() {
    local name="${1%.out}"
    local result="$RESULTS_DIR/$name.out"
    local expected="$EXPECTED_DIR/$name.out"

    [ -f "$result" ] || { echo "  SKIP  $name (no result)"; return 1; }

    if [ -f "$expected" ] && diff -q "$expected" "$result" > /dev/null; then
        echo "  SAME  $name"
    else
        echo "  DIFF  $name"
    fi
    cp "$result" "$expected"
}

[ $# -eq 0 ] && usage 0
mkdir -p "$EXPECTED_DIR"

case "$1" in
    -h|--help)  usage 0 ;;
    -l|--list)  list_tests ;;
    -a|--all)
        [ -d "$RESULTS_DIR" ] || { echo "Run ./run.sh first"; exit 1; }
        for f in "$RESULTS_DIR"/*.out; do
            [ -f "$f" ] && update_one "$(basename "$f")"
        done
        ;;
    -*)
        usage 1 ;;
    *)
        for name in "$@"; do
            update_one "$name"
        done
        ;;
esac

