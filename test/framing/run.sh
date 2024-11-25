#!/usr/bin/env bash

# RawTherapee CLI
CLI=$1
# Directory to search for tests (cwd by default)
TEST_DIR="${2:-.}"
# Image to use as test input
IMAGE=$3
# Output directory for converted images
OUTDIR=$4
# Specify -Y to pass to CLI
OVERRIDE=$5

# Loop through all .pp3 files in the specified directory
for FILE in "$TEST_DIR"/*.pp3; do
    # Check if any .pp3 files exist
    if [[ -e "$FILE" ]]; then
        ABSOLUTE_PATH=$(realpath $FILE)
        TESTCASE="${FILE%.*}"
        echo "$CLI -d -p $ABSOLUTE_PATH -o $OUTDIR/${TESTCASE}.jpg $OVERRIDE -c $IMAGE"
        $CLI -d -p $ABSOLUTE_PATH -o $OUTDIR/${TESTCASE}.jpg $OVERRIDE -c $IMAGE
    else
        echo "No .pp3 files found in $TEST_DIR"
        break
    fi
done
