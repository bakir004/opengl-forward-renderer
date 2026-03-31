#!/usr/bin/env bash
set -e

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
            cp "$hook_path" "$HOOK_DEST_DIR/$hook_file"
            chmod +x "$HOOK_DEST_DIR/$hook_file"

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

# Ensure build directory exists
mkdir -p "$BUILD_DIR"

cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$CONFIG" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build "$BUILD_DIR" --config "$CONFIG" --parallel "$(nproc 2>/dev/null || sysctl -n hw.logicalcpu)" --target RendererLib
cmake --build "$BUILD_DIR" --config "$CONFIG" --parallel "$(nproc 2>/dev/null || sysctl -n hw.logicalcpu)" --target TestApp

# Symlink compile_commands for Neovim/LSP
ln -sf "$BUILD_DIR/compile_commands.json" . 2>/dev/null || true

echo "Build successful. Running..."
EXECUTABLE="$BUILD_DIR/test-app/TestApp"
if [ -f "$EXECUTABLE" ]; then
    "$EXECUTABLE"
else
    echo "Error: TestApp executable not found at $EXECUTABLE"
    exit 1
fi
