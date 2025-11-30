# Basic compliance tests: --help, --version

section "basic"

# Test --help
output=$("$TRY_BIN" --help 2>&1)
if echo "$output" | grep -q "ephemeral workspace manager"; then
    pass
else
    fail "--help missing expected text" "contains 'ephemeral workspace manager'" "$output"
fi

# Test -h
output=$("$TRY_BIN" -h 2>&1)
if echo "$output" | grep -q "ephemeral workspace manager"; then
    pass
else
    fail "-h missing expected text" "contains 'ephemeral workspace manager'" "$output"
fi

# Test --version
output=$("$TRY_BIN" --version 2>&1)
if echo "$output" | grep -qE "^try [0-9]+\.[0-9]+"; then
    pass
else
    fail "--version format incorrect" "try X.Y.Z" "$output"
fi

# Test -v
output=$("$TRY_BIN" -v 2>&1)
if echo "$output" | grep -qE "^try [0-9]+\.[0-9]+"; then
    pass
else
    fail "-v format incorrect" "try X.Y.Z" "$output"
fi

# Test unknown command shows help
output=$("$TRY_BIN" unknowncommand 2>&1 || true)
if echo "$output" | grep -q "Unknown command"; then
    pass
else
    fail "unknown command should show error" "contains 'Unknown command'" "$output"
fi
