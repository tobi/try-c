#!/bin/bash
# Spec compliance test runner for try
# Usage: ./runner.sh /path/to/try

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SPEC_DIR="$(dirname "$SCRIPT_DIR")"

# Check arguments
if [ $# -lt 1 ]; then
    echo "Usage: $0 /path/to/try"
    echo "Run spec compliance tests against a try binary"
    exit 1
fi

TRY_BIN="$1"

# Verify binary exists and is executable
if [ ! -x "$TRY_BIN" ]; then
    echo -e "${RED}Error: '$TRY_BIN' is not executable or does not exist${NC}"
    exit 1
fi

# Export for test scripts
export TRY_BIN
export SPEC_DIR

# Create test environment
export TEST_ROOT=$(mktemp -d)
export TEST_TRIES="$TEST_ROOT/tries"
mkdir -p "$TEST_TRIES"

# Create test directories with different mtimes
mkdir -p "$TEST_TRIES/2025-11-01-alpha"
mkdir -p "$TEST_TRIES/2025-11-15-beta"
mkdir -p "$TEST_TRIES/2025-11-20-gamma"
mkdir -p "$TEST_TRIES/2025-11-25-project-with-long-name"
mkdir -p "$TEST_TRIES/no-date-prefix"

# Set mtimes (oldest first)
touch -d "2025-11-01" "$TEST_TRIES/2025-11-01-alpha"
touch -d "2025-11-15" "$TEST_TRIES/2025-11-15-beta"
touch -d "2025-11-20" "$TEST_TRIES/2025-11-20-gamma"
touch -d "2025-11-25" "$TEST_TRIES/2025-11-25-project-with-long-name"
touch "$TEST_TRIES/no-date-prefix"  # Most recent

# Counters
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

# Test utilities - exported for test scripts
pass() {
    echo -en "${GREEN}.${NC}"
    TESTS_PASSED=$((TESTS_PASSED + 1))
    TESTS_RUN=$((TESTS_RUN + 1))
}

fail() {
    echo -e "\n${RED}FAIL${NC}: $1"
    if [ -n "$2" ]; then
        echo "  Expected: $2"
    fi
    if [ -n "$3" ]; then
        echo "  Got: $3"
    fi
    TESTS_FAILED=$((TESTS_FAILED + 1))
    TESTS_RUN=$((TESTS_RUN + 1))
}

section() {
    echo -en "\n${YELLOW}$1${NC} "
}

export -f pass fail section

# Cleanup on exit
cleanup() {
    rm -rf "$TEST_ROOT"
}
trap cleanup EXIT

# Header
echo "Testing: $TRY_BIN"
echo "Spec dir: $SPEC_DIR"
echo "Test env: $TEST_TRIES"
echo

# Run all test_*.sh files in order
for test_file in "$SCRIPT_DIR"/test_*.sh; do
    if [ -f "$test_file" ]; then
        # Source the test file to run in same environment
        source "$test_file"
    fi
done

# Summary
echo
echo
echo "═══════════════════════════════════"
echo "Results: $TESTS_PASSED/$TESTS_RUN passed"
if [ $TESTS_FAILED -gt 0 ]; then
    echo -e "${RED}$TESTS_FAILED tests failed${NC}"
    exit 1
else
    echo -e "${GREEN}All tests passed${NC}"
    exit 0
fi
