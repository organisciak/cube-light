#!/usr/bin/env bash
# Build the native pattern harness (no PlatformIO needed — plain clang++).
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p build
clang++ -std=c++17 -O2 -Wall -I../lib/core ../lib/core/*.cpp main.cpp -o build/cube-native
echo "built: $(pwd)/build/cube-native"
