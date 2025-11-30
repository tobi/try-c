#!/bin/bash
# Script to test the TUI with simulated input
# Simulates typing "test" then ESC
echo -e "test\x1b" | valgrind --leak-check=full --track-origins=yes ./try 2>&1
