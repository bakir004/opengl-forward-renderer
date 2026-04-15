#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

RUN_EXE=""
BUILD_DIR="$SCRIPT_DIR/build"

if [ -x "$BUILD_DIR/test-app/Debug/TestApp" ]; then
    RUN_EXE="$BUILD_DIR/test-app/Debug/TestApp"
elif [ -x "$BUILD_DIR/test-app/Release/TestApp" ]; then
    RUN_EXE="$BUILD_DIR/test-app/Release/TestApp"
elif [ -x "$BUILD_DIR/test-app/TestApp" ]; then
    RUN_EXE="$BUILD_DIR/test-app/TestApp"
else
    RUN_EXE="$(find "$BUILD_DIR" -type f -name TestApp -perm -111 2>/dev/null | head -n 1 || true)"
fi

if [ -z "$RUN_EXE" ] || [ ! -x "$RUN_EXE" ]; then
    echo "Error: no built TestApp found under $BUILD_DIR — run ./build.sh first"
    exit 1
fi

"$RUN_EXE"
