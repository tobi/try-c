#!/bin/bash
# Simulate typing "ab" then ESC
printf "ab\x1b" | valgrind --leak-check=full --track-origins=yes ./try 2>&1 | tail -80
