#!/bin/bash
set -e

BUILD_DIR="build"
CONFIG="${1:-Debug}"

cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$CONFIG" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build "$BUILD_DIR" --config "$CONFIG" --parallel "$(nproc 2>/dev/null || sysctl -n hw.logicalcpu)"

ln -sf "$BUILD_DIR/compile_commands.json" . 2>/dev/null || true

echo "Build successful. Running..."
"$BUILD_DIR/ForwardRenderer"