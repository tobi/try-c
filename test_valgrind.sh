#!/bin/bash
# Test with just Enter key
echo "" | timeout 2 valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./try 2>&1 | grep -A 20 "ERROR SUMMARY\|Invalid\|uninitialised\|heap"
