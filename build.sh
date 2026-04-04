#!/usr/bin/env bash
set -euo pipefail

# --- Git Hooks Setup ---
HOOK_SRC_DIR=".githooks"
HOOK_DEST_DIR=".git/hooks"

if [ -d ".git" ]; then
    if [ -d "$HOOK_SRC_DIR" ]; then
        echo "Installing Git hooks from $HOOK_SRC_DIR..."

        # Install all bash hooks from .githooks
        for hook_path in "$HOOK_SRC_DIR"/*.sh; do
            [ -e "$hook_path" ] || break
            # Get just the filename (e.g., commit-msg-hook.sh -> commit-msg)
            # We strip the directory and the .sh extension for Git to recognize it
            hook_file=$(basename "$hook_path" .sh)

            # Copy to .git/hooks and make executable
            install -m 755 "$hook_path" "$HOOK_DEST_DIR/$hook_file"

            echo " -> Installed $hook_file"
        done
        echo "All hooks installed successfully."
    else
        echo "Warning: $HOOK_SRC_DIR not found. Skipping hook installation."
    fi
else
    echo "Warning: .git directory not found. Skipping hook installation."
fi

# --- Build Logic ---
BUILD_DIR="build"
CONFIG="${1:-Debug}"

detect_jobs() {
    if command -v nproc >/dev/null 2>&1; then
        nproc
        return
    fi

    if command -v getconf >/dev/null 2>&1; then
        getconf _NPROCESSORS_ONLN
        return
    fi

    if command -v sysctl >/dev/null 2>&1; then
        sysctl -n hw.logicalcpu 2>/dev/null || echo 1
        return
    fi

    echo 1
}

JOBS="$(detect_jobs)"
if ! [[ "$JOBS" =~ ^[0-9]+$ ]] || [ "$JOBS" -lt 1 ]; then
    JOBS=1
fi

GENERATOR="${CMAKE_GENERATOR:-}"
if [ -z "$GENERATOR" ]; then
    if command -v ninja >/dev/null 2>&1; then
        GENERATOR="Ninja"
    else
        GENERATOR="Unix Makefiles"
    fi
fi

if [ "$GENERATOR" = "Ninja" ] && ! command -v ninja >/dev/null 2>&1; then
    echo "Error: CMAKE_GENERATOR=Ninja is set, but 'ninja' is not installed."
    exit 1
fi

if [ "$GENERATOR" = "Unix Makefiles" ] && ! command -v make >/dev/null 2>&1; then
    echo "Error: Unix Makefiles generator selected, but 'make' is not installed."
    exit 1
fi

if [ -f "$BUILD_DIR/CMakeCache.txt" ]; then
    CACHED_GENERATOR="$(grep -E '^CMAKE_GENERATOR:INTERNAL=' "$BUILD_DIR/CMakeCache.txt" | sed 's/^CMAKE_GENERATOR:INTERNAL=//' || true)"
    if [ -n "$CACHED_GENERATOR" ] && [ "$CACHED_GENERATOR" != "$GENERATOR" ]; then
        echo "Existing CMake cache uses generator '$CACHED_GENERATOR'. Recreating $BUILD_DIR..."
        rm -rf "$BUILD_DIR"
    fi
fi

# Ensure build directory exists
mkdir -p "$BUILD_DIR"

echo "Configuring with generator: $GENERATOR"
cmake -S . -B "$BUILD_DIR" -G "$GENERATOR" -DCMAKE_BUILD_TYPE="$CONFIG" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

echo "Building with $JOBS parallel jobs..."
cmake --build "$BUILD_DIR" --config "$CONFIG" --parallel "$JOBS"

# Symlink compile_commands for Neovim/LSP
ln -sf "$BUILD_DIR/compile_commands.json" . 2>/dev/null || true

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
    echo "Error: Built executable not found. Expected TestApp under $BUILD_DIR output directories."
    exit 1
fi
