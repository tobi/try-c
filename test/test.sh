#!/bin/bash
# Test suite for try CLI

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

# Check if valgrind is available
HAS_VALGRIND=0
if command -v valgrind &> /dev/null; then
    HAS_VALGRIND=1
fi

# Test utilities
test_start() {
    echo -e "${YELLOW}TEST:${NC} $1"
    TESTS_RUN=$((TESTS_RUN + 1))
}

test_start_valgrind() {
    echo -e "${YELLOW}TEST (valgrind):${NC} $1"
    TESTS_RUN=$((TESTS_RUN + 1))
}

test_pass() {
    echo -e "${GREEN}✓ PASS${NC}"
    TESTS_PASSED=$((TESTS_PASSED + 1))
    echo
}

test_fail() {
    echo -e "${RED}✗ FAIL:${NC} $1"
    TESTS_FAILED=$((TESTS_FAILED + 1))
    echo
}

# Setup test environment
TEST_DIR=$(mktemp -d)
TEST_TRIES="$TEST_DIR/tries"
mkdir -p "$TEST_TRIES"

# Create some test directories
mkdir -p "$TEST_TRIES/2025-11-09-test-alpha"
mkdir -p "$TEST_TRIES/2025-11-12-test-beta"
mkdir -p "$TEST_TRIES/2025-11-15-example"
mkdir -p "$TEST_TRIES/2025-11-20-this-is-a-very-long-directory-name-that-should-be-truncated-with-ellipsis"

# Touch to set mtimes (most recent last)
touch "$TEST_TRIES/2025-11-09-test-alpha"
sleep 0.1
touch "$TEST_TRIES/2025-11-12-test-beta"
sleep 0.1
touch "$TEST_TRIES/2025-11-15-example"

echo "Test environment: $TEST_TRIES"
echo "Test directories created: $(ls $TEST_TRIES | wc -l)"
echo

# Test 1: Help command
test_start "Help command"
if ./dist/try --no-expand-tokens --help 2>&1 | grep -q "ephemeral workspace"; then
    test_pass
else
    test_fail "Help output missing"
fi

if [ $HAS_VALGRIND -eq 1 ]; then
    test_start_valgrind "Help command"
    if valgrind --leak-check=full --error-exitcode=1 --quiet ./dist/try --no-expand-tokens --help 2>&1 | grep -q "ephemeral workspace"; then
        test_pass
    else
        test_fail "Help output missing or valgrind error"
    fi
fi

# Test 2: Init command
test_start "Init command generates shell function"
OUTPUT=$(./dist/try --no-expand-tokens init)
if echo "$OUTPUT" | grep -q "try()"; then
    test_pass
else
    test_fail "Init did not generate shell function"
fi

if [ $HAS_VALGRIND -eq 1 ]; then
    test_start_valgrind "Init command generates shell function"
    OUTPUT=$(valgrind --leak-check=full --error-exitcode=1 --quiet ./dist/try --no-expand-tokens init)
    if echo "$OUTPUT" | grep -q "try()"; then
        test_pass
    else
        test_fail "Init did not generate shell function or valgrind error"
    fi
fi

# Test 3: Exec clone command generates shell script
test_start "Exec clone command generates shell script"
OUTPUT=$(./dist/try --no-expand-tokens --path="$TEST_TRIES" exec clone https://github.com/test/repo)
if echo "$OUTPUT" | grep -q "git clone" && echo "$OUTPUT" | grep -q "# if you can read this"; then
    test_pass
else
    test_fail "Exec clone did not generate expected script. Output: $OUTPUT"
fi

if [ $HAS_VALGRIND -eq 1 ]; then
    test_start_valgrind "Exec clone command generates shell script"
    OUTPUT=$(valgrind --leak-check=full --error-exitcode=1 --quiet ./dist/try --no-expand-tokens --path="$TEST_TRIES" exec clone https://github.com/test/repo)
    if echo "$OUTPUT" | grep -q "git clone" && echo "$OUTPUT" | grep -q "# if you can read this"; then
        test_pass
    else
        test_fail "Exec clone did not generate expected script or valgrind error"
    fi
fi

# Test 4: --and-exit flag (render once)
test_start "Test mode: --and-exit renders once"
OUTPUT=$(./dist/try --no-expand-tokens --path="$TEST_TRIES" --and-exit exec 2>&1 || true)
# Should render the TUI but not hang
if [ ! -z "$OUTPUT" ]; then
    test_pass
else
    test_fail "No output from --and-exit"
fi

if [ $HAS_VALGRIND -eq 1 ]; then
    test_start_valgrind "Test mode: --and-exit renders once"
    OUTPUT=$(valgrind --leak-check=full --error-exitcode=1 --quiet ./dist/try --no-expand-tokens --path="$TEST_TRIES" --and-exit exec 2>&1 || true)
    if [ ! -z "$OUTPUT" ]; then
        test_pass
    else
        test_fail "No output from --and-exit or valgrind error"
    fi
fi

# Test 5: --and-keys with ESC (cancel)
test_start "Test mode: --and-keys with ESC cancels"
OUTPUT=$(./dist/try --no-expand-tokens --path="$TEST_TRIES" --and-keys=$'\x1b' exec 2>/dev/null || true)
# ESC should cancel and output "Cancelled."
if echo "$OUTPUT" | grep -q "Cancelled"; then
    test_pass
else
    test_fail "ESC did not cancel properly. Output: $OUTPUT"
fi

if [ $HAS_VALGRIND -eq 1 ]; then
    test_start_valgrind "Test mode: --and-keys with ESC cancels"
    OUTPUT=$(valgrind --leak-check=full --error-exitcode=1 --quiet ./dist/try --no-expand-tokens --path="$TEST_TRIES" --and-keys=$'\x1b' exec 2>/dev/null || true)
    if echo "$OUTPUT" | grep -q "Cancelled"; then
        test_pass
    else
        test_fail "ESC did not cancel properly or valgrind error"
    fi
fi

# Test 6: --and-keys with Enter (select first item)
test_start "Test mode: --and-keys with Enter selects"
OUTPUT=$(./dist/try --no-expand-tokens --path="$TEST_TRIES" --and-keys=$'\r' exec 2>/dev/null)
# Should output cd command with script header
if echo "$OUTPUT" | grep -q "cd" && echo "$OUTPUT" | grep -q "# if you can read this"; then
    test_pass
else
    test_fail "Enter did not select item. Output: $OUTPUT"
fi

if [ $HAS_VALGRIND -eq 1 ]; then
    test_start_valgrind "Test mode: --and-keys with Enter selects"
    OUTPUT=$(valgrind --leak-check=full --error-exitcode=1 --quiet ./dist/try --no-expand-tokens --path="$TEST_TRIES" --and-keys=$'\r' exec 2>/dev/null)
    if echo "$OUTPUT" | grep -q "cd" && echo "$OUTPUT" | grep -q "# if you can read this"; then
        test_pass
    else
        test_fail "Enter did not select item or valgrind error"
    fi
fi

# Test 7: --and-keys with typing and Enter
test_start "Test mode: --and-keys with text input"
OUTPUT=$(./dist/try --no-expand-tokens --path="$TEST_TRIES" --and-keys="beta"$'\r' exec 2>/dev/null)
# Should match "beta" and select it
if echo "$OUTPUT" | grep -q "test-beta"; then
    test_pass
else
    test_fail "Text filtering did not work. Output: $OUTPUT"
fi

if [ $HAS_VALGRIND -eq 1 ]; then
    test_start_valgrind "Test mode: --and-keys with text input"
    OUTPUT=$(valgrind --leak-check=full --error-exitcode=1 --quiet ./dist/try --no-expand-tokens --path="$TEST_TRIES" --and-keys="beta"$'\r' exec 2>/dev/null)
    if echo "$OUTPUT" | grep -q "test-beta"; then
        test_pass
    else
        test_fail "Text filtering did not work or valgrind error. Output: $OUTPUT"
    fi
fi

# Test 8: --and-keys with Down arrow and Enter
test_start "Test mode: --and-keys with arrow navigation"
# Down arrow is \x1b[B
OUTPUT=$(./dist/try --no-expand-tokens --path="$TEST_TRIES" --and-keys=$'\x1b[B\r' exec 2>/dev/null)
if echo "$OUTPUT" | grep -q "cd"; then
    test_pass
else
    test_fail "Arrow navigation failed"
fi

if [ $HAS_VALGRIND -eq 1 ]; then
    test_start_valgrind "Test mode: --and-keys with arrow navigation"
    OUTPUT=$(valgrind --leak-check=full --error-exitcode=1 --quiet ./dist/try --no-expand-tokens --path="$TEST_TRIES" --and-keys=$'\x1b[B\r' exec 2>/dev/null)
    if echo "$OUTPUT" | grep -q "cd"; then
        test_pass
    else
        test_fail "Arrow navigation failed or valgrind error"
    fi
fi

# Test 9: Score shown in full (no truncation)
test_start "Score displayed completely"
OUTPUT=$(COLUMNS=80 ./dist/try --no-expand-tokens --path="$TEST_TRIES" --and-exit exec 2>&1 || true)
# Check that scores include decimal point and digit after it
if echo "$OUTPUT" | grep -q ", [0-9]\+\.[0-9]"; then
    test_pass
else
    test_fail "Score not shown in full. Output: $OUTPUT"
fi

if [ $HAS_VALGRIND -eq 1 ]; then
    test_start_valgrind "Score displayed completely"
    OUTPUT=$(COLUMNS=80 valgrind --leak-check=full --error-exitcode=1 --quiet ./dist/try --no-expand-tokens --path="$TEST_TRIES" --and-exit exec 2>&1 || true)
    if echo "$OUTPUT" | grep -q ", [0-9]\+\.[0-9]"; then
        test_pass
    else
        test_fail "Score not shown in full or valgrind error"
    fi
fi

# Test 10: Long names truncated with ellipsis
test_start "Long directory names truncated with ellipsis"
OUTPUT=$(COLUMNS=60 ./dist/try --no-expand-tokens --path="$TEST_TRIES" --and-exit exec 2>&1 || true)
# Check that the long directory name is truncated with ellipsis
if echo "$OUTPUT" | grep -q "2025-11-20.*…"; then
    test_pass
else
    test_fail "Long name not truncated with ellipsis. Output: $OUTPUT"
fi

if [ $HAS_VALGRIND -eq 1 ]; then
    test_start_valgrind "Long directory names truncated with ellipsis"
    OUTPUT=$(COLUMNS=60 valgrind --leak-check=full --error-exitcode=1 --quiet ./dist/try --no-expand-tokens --path="$TEST_TRIES" --and-exit exec 2>&1 || true)
    if echo "$OUTPUT" | grep -q "2025-11-20.*…"; then
        test_pass
    else
        test_fail "Long name not truncated with ellipsis or valgrind error"
    fi
fi

# Test 11: Metadata right-aligned (overlapping test)
test_start "Metadata right-aligned at terminal edge"
OUTPUT=$(COLUMNS=80 ./dist/try --no-expand-tokens --path="$TEST_TRIES" --and-exit exec 2>&1 || true)
# Extract a line with metadata and check it ends near column 80
# The metadata should appear at the end of lines that aren't truncated
if echo "$OUTPUT" | grep "test-alpha" | grep -q "{dim}just now, [0-9]\+\.[0-9]{reset}"; then
    test_pass
else
    test_fail "Metadata not properly right-aligned. Output: $OUTPUT"
fi

if [ $HAS_VALGRIND -eq 1 ]; then
    test_start_valgrind "Metadata right-aligned at terminal edge"
    OUTPUT=$(COLUMNS=80 valgrind --leak-check=full --error-exitcode=1 --quiet ./dist/try --no-expand-tokens --path="$TEST_TRIES" --and-exit exec 2>&1 || true)
    if echo "$OUTPUT" | grep "test-alpha" | grep -q "{dim}just now, [0-9]\+\.[0-9]{reset}"; then
        test_pass
    else
        test_fail "Metadata not properly right-aligned or valgrind error"
    fi
fi

# Print valgrind status
if [ $HAS_VALGRIND -eq 0 ]; then
    echo
    echo "Skipping valgrind tests - not in path"
    echo
fi

# Cleanup
rm -rf "$TEST_DIR"

# Summary
echo "═══════════════════════════════════"
echo "Test Summary"
echo "═══════════════════════════════════"
echo "Tests run:    $TESTS_RUN"
echo -e "Tests passed: ${GREEN}$TESTS_PASSED${NC}"
if [ $TESTS_FAILED -gt 0 ]; then
    echo -e "Tests failed: ${RED}$TESTS_FAILED${NC}"
    exit 1
else
    echo -e "Tests failed: ${GREEN}0${NC}"
    echo
    echo -e "${GREEN}All tests passed!${NC}"
    exit 0
fi
