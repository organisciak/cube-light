#!/usr/bin/env bash
# Compile the firmware pattern engine (firmware/lib/core, unchanged) to
# WebAssembly for the browser simulator. Needs emscripten (`brew install
# emscripten`). Output: web/engine.js + web/engine.wasm (committed, so the
# page works from GitHub Pages without a toolchain).
set -euo pipefail
cd "$(dirname "$0")"
CORE=../firmware/lib/core
EXPORTS=$(grep -o 'EMSCRIPTEN_KEEPALIVE [a-z_ *0-9]* \*\?cube_[a-z_0-9]*' wasm/bridge.cpp \
  | grep -o 'cube_[a-z_0-9]*' | sed 's/^/"_/;s/$/"/' | paste -sd, -)
em++ -std=c++17 -O3 -I"$CORE" "$CORE"/*.cpp wasm/bridge.cpp \
  -o web/engine.js \
  -s MODULARIZE=1 -s EXPORT_ES6=1 -s EXPORT_NAME=createCubeEngine \
  -s ENVIRONMENT=web -s ALLOW_MEMORY_GROWTH=0 -s INITIAL_MEMORY=16MB \
  -s EXPORTED_FUNCTIONS="[$EXPORTS,\"_malloc\",\"_free\"]" \
  -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap","UTF8ToString","HEAPU8","HEAPF32"]' \
  -s FILESYSTEM=0 -s ASSERTIONS=0
ls -la web/engine.js web/engine.wasm
