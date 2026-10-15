#!/bin/bash

# Fail fast and prefer the runtime rebuilt for the active CMake workflow.
set -eu
DEFAULT_RUNTIME_DIR=".cache/build/default/lib"
if [ ! -d "$DEFAULT_RUNTIME_DIR" ]; then DEFAULT_RUNTIME_DIR="lib"; fi
RUNTIME_DIR="${SYSY_RUNTIME_DIR:-$DEFAULT_RUNTIME_DIR}"

INPUT_FILE="${1:-test.s}"
OUTPUT_BIN="${2:-test.bin}"

if [ ! -f "$INPUT_FILE" ]; then
    echo "Error: Input file '$INPUT_FILE' not found"
    exit 1
fi

aarch64-linux-gnu-gcc "$INPUT_FILE" -o "$OUTPUT_BIN" -L"$RUNTIME_DIR" -lsysy_aarch -static -g

echo "Successfully compiled $INPUT_FILE to $OUTPUT_BIN"
