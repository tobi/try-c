#!/bin/bash
# Run with valgrind and capture full output
printf "ab\x1b" | valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose ./try 2>&1 | tee valgrind_output.txt
