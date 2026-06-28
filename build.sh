#!/usr/bin/env bash
set -euo pipefail

# ==============================
# CONFIG
# ==============================
BUILD_DIR="build"
CONFIG="${1:-Release}"

# Normalize build type
if [[ "$CONFIG" == "debug" || "$CONFIG" == "Debug" ]]; then
    CONFIG="Debug"
else
    CONFIG="Release"
fi

# ==============================
# GIT HOOKS (OPTIONAL)
# ==============================
HOOK_SRC_DIR=".githooks"
HOOK_DEST_DIR=".git/hooks"

if [ -d ".git" ] && [ -d "$HOOK_SRC_DIR" ]; then
    echo "Installing Git hooks from $HOOK_SRC_DIR..."

    for hook_path in "$HOOK_SRC_DIR"/*.sh; do
        [ -e "$hook_path" ] || continue

        hook_file=$(basename "$hook_path" .sh)
        install -m 755 "$hook_path" "$HOOK_DEST_DIR/$hook_file"

        echo " -> Installed $hook_file"
    done
fi

# ==============================
# GENERATOR SELECTION (CMake-style)
# ==============================
GENERATOR="${CMAKE_GENERATOR:-}"

if [ -z "$GENERATOR" ]; then
    if command -v ninja >/dev/null 2>&1; then
        GENERATOR="Ninja"
    else
        GENERATOR="Unix Makefiles"
    fi
fi

echo "Using generator: $GENERATOR"
echo "Build type: $CONFIG"

# ==============================
# CONFIGURE
# ==============================
cmake -S . -B "$BUILD_DIR" \
    -G "$GENERATOR" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# ==============================
# BUILD
# ==============================
cmake --build "$BUILD_DIR" --config "$CONFIG"

# ==============================
# LINK compile_commands.json (for LSP)
# ==============================
ln -sf "$BUILD_DIR/compile_commands.json" . 2>/dev/null || true

# ==============================
# RUN
# ==============================
echo "Build successful. Running..."

RUN_EXE=""

if [ -x "$BUILD_DIR/test-app/$CONFIG/TestApp" ]; then
    RUN_EXE="$BUILD_DIR/test-app/$CONFIG/TestApp"
elif [ -x "$BUILD_DIR/test-app/TestApp" ]; then
    RUN_EXE="$BUILD_DIR/test-app/TestApp"
elif [ -x "$BUILD_DIR/$CONFIG/TestApp" ]; then
    RUN_EXE="$BUILD_DIR/$CONFIG/TestApp"
elif [ -x "$BUILD_DIR/TestApp" ]; then
    RUN_EXE="$BUILD_DIR/TestApp"
else
    RUN_EXE="$(find "$BUILD_DIR" -type f -name TestApp -perm -111 2>/dev/null | head -n 1 || true)"
fi

if [ -n "$RUN_EXE" ] && [ -x "$RUN_EXE" ]; then
    "$RUN_EXE"
else
    echo "Error: TestApp executable not found."
    exit 1
fi