# Test that testing parameters exist (required for spec compliance testing)
# See ../command_line.md "Testing and Debugging" section

section "test-params"

# Test --and-exit exists (renders TUI once and exits)
output=$("$TRY_BIN" --path="$TEST_TRIES" --and-exit exec 2>&1 || true)
if [ -n "$output" ]; then
    pass
else
    fail "--and-exit not working" \
        "TUI output" \
        "empty output (see $SPEC_DIR/command_line.md Testing section)"
fi

# Test --and-keys exists (inject keys)
# ESC should cancel and output "Cancelled."
output=$("$TRY_BIN" --path="$TEST_TRIES" --and-keys=$'\x1b' exec 2>/dev/null || true)
if echo "$output" | grep -qi "cancel"; then
    pass
else
    fail "--and-keys not working (ESC should cancel)" \
        "contains 'cancel'" \
        "$output (see $SPEC_DIR/command_line.md Testing section)"
fi

# Test --and-keys with Enter (should select and output cd script)
output=$("$TRY_BIN" --path="$TEST_TRIES" --and-keys=$'\r' exec 2>/dev/null || true)
if echo "$output" | grep -q "cd '"; then
    pass
else
    fail "--and-keys not working (Enter should select)" \
        "contains cd command" \
        "$output (see $SPEC_DIR/command_line.md Testing section)"
fi
