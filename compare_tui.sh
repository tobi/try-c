#!/bin/bash
# compare_tui.sh

mkdir -p test_env/tries
cd test_env

# Create some dummy tries
mkdir -p tries/2025-01-01-alpha
mkdir -p tries/2025-01-02-beta

# Capture Ruby output (simulated interaction)
# We use 'script' command or just pipe? Ruby script checks for TTY.
# We might need to force it or mock it.
# The ruby script uses `STDERR.tty?`
# We can use `unbuffer` or `script` to fake TTY.
# Or just run it and see if it outputs ANSI codes even if not TTY?
# Looking at ruby code:
# out = if STDOUT.tty? UI.expand_tokens(text) else text.gsub(...) end
# But the TUI writes to STDERR.
# UI.puts writes to STDERR.
# We need to capture STDERR.

echo "Capturing Ruby TUI..."
# We use a small ruby wrapper to force TTY if needed, or just run it.
# Let's try running it with a pipe and see if it degrades.
# If it degrades, we might need `script`.
# `script -q -c "/usr/bin/try --path $(pwd)/tries" ruby_out.log`
# But `script` output contains timing info and CRs.

# Let's try to just run it and capture stderr, hoping it respects force color or similar?
# The ruby script doesn't seem to have a force color flag.
# It checks `STDERR.tty?`.

# We can use python's pty module to run it?
# Or just `script`.
script -q -c "/usr/bin/try --path $(pwd)/tries" ruby_out.raw > /dev/null
# Clean up typescript file (remove first line "Script started..." and last line)
# Actually `script` output format varies.
# Let's just look at the raw output.

echo "Capturing C TUI..."
script -q -c "../try --path $(pwd)/tries" c_out.raw > /dev/null

echo "--- Ruby Output (hexdump head) ---"
head -n 20 ruby_out.raw | hexdump -C

echo "--- C Output (hexdump head) ---"
head -n 20 c_out.raw | hexdump -C

# Also save readable versions
cat -v ruby_out.raw > ruby_out.txt
cat -v c_out.raw > c_out.txt

echo "Done. Check ruby_out.raw and c_out.raw"
