#!/usr/bin/env bash

# Program: Archfetch
# Language ; C
# Author: Xzrayツ

#  ==============================================================================  #


set -euo pipefail

SOURCE_FILE="archfetch.c"
OUTPUT_BINARY="archfetch"

CFLAGS=(
  -march=native
  -mtune=native
  -O3
)

# Check if clang is available
if ! command -v clang >/dev/null 2>&1; then
  echo "Error: clang compiler not found." >&2
  exit 1
fi

# Check if source file exists
if [[ ! -f "$SOURCE_FILE" ]]; then
  echo "Error: Source file '$SOURCE_FILE' not found." >&2
  exit 1
fi

# Build
clang "${CFLAGS[@]}" "$SOURCE_FILE" -o "$OUTPUT_BINARY"

echo "Build completed successfully: ./$OUTPUT_BINARY"
