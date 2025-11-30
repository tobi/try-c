# Script output format compliance tests

section "script-format"

# Test: clone script format
output=$("$TRY_BIN" --path="$TEST_TRIES" exec clone https://github.com/user/repo 2>&1)

# Should have warning header
if echo "$output" | head -1 | grep -q "^#"; then
    pass
else
    fail "clone script should start with comment" "# comment" "$(echo "$output" | head -1)"
fi

# Should have git clone command
if echo "$output" | grep -q "git clone 'https://github.com/user/repo'"; then
    pass
else
    fail "clone script should have git clone with URL" "git clone 'url'" "$output"
fi

# Should chain with && \
if echo "$output" | grep -q "&& \\\\"; then
    pass
else
    fail "commands should chain with && \\\\" "found && \\\\" "$output"
fi

# Should end with true
if echo "$output" | grep -q "true$"; then
    pass
else
    fail "script should end with 'true'" "ends with true" "$output"
fi

# Test: cd script format (select existing directory)
output=$("$TRY_BIN" --path="$TEST_TRIES" --and-keys=$'\r' exec 2>/dev/null)

# Should touch the directory
if echo "$output" | grep -q "touch '"; then
    pass
else
    fail "cd script should touch directory" "touch command" "$output"
fi

# Should cd to directory
if echo "$output" | grep -q "cd '$TEST_TRIES/"; then
    pass
else
    fail "cd script should cd to tries path" "cd to test path" "$output"
fi
