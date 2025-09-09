#!/bin/bash

# Start python3 auxpow_mining.py
OUTPUT=$(python3 auxpow_mining.py 2>&1)

# Get test directory from the output
TEST_DIR=$(echo "$OUTPUT" | grep -oP 'TestFramework $$\$\$ERROR$$\: Test failed. Test logging available at \/\K[^\s]+')

# Get script run number
N=$(ls -d logs* | wc -l)

# Call combine_logs.py and save output to logsN.txt
/workspaces/bellscoinV3/test/functional/combine_logs.py "$TEST_DIR" > "logs$N.txt"