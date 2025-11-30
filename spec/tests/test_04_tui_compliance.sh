# TUI behavior compliance tests

section "tui"

# Test: ESC cancels with exit code 1
set +e
"$TRY_BIN" --path="$TEST_TRIES" --and-keys=$'\x1b' exec >/dev/null 2>&1
exit_code=$?
set -e
if [ $exit_code -eq 1 ]; then
    pass
else
    fail "ESC should exit with code 1" "exit code 1" "exit code $exit_code"
fi

# Test: Enter selects with exit code 0
set +e
"$TRY_BIN" --path="$TEST_TRIES" --and-keys=$'\r' exec >/dev/null 2>&1
exit_code=$?
set -e
if [ $exit_code -eq 0 ]; then
    pass
else
    fail "Enter should exit with code 0" "exit code 0" "exit code $exit_code"
fi

# Test: Typing filters results
output=$("$TRY_BIN" --path="$TEST_TRIES" --and-keys="beta"$'\r' exec 2>/dev/null) || true
if echo "$output" | grep -q "beta"; then
    pass
else
    fail "typing 'beta' should select beta directory" "path contains 'beta'" "$output"
fi

# Test: Arrow navigation works (down then enter)
output=$("$TRY_BIN" --path="$TEST_TRIES" --and-keys=$'\x1b[B\r' exec 2>/dev/null) || true
if echo "$output" | grep -q "cd '"; then
    pass
else
    fail "down arrow + enter should select" "cd command" "$output"
fi

# Test: Script output format (touch, cd, true chain)
output=$("$TRY_BIN" --path="$TEST_TRIES" --and-keys=$'\r' exec 2>/dev/null) || true
if echo "$output" | grep -q "touch '" && echo "$output" | grep -q "&& \\\\" && echo "$output" | grep -q "true"; then
    pass
else
    fail "script should chain touch && cd && true" "touch ... && cd ... && true" "$output"
fi

# Test: Script has warning header
if echo "$output" | grep -q "# if you can read this"; then
    pass
else
    fail "script should have warning header" "comment about alias" "$output"
fi
