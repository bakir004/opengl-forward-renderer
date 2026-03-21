#!/bin/bash
set -e

# --- Git Hooks Setup ---
HOOK_DEST=".git/hooks/commit-msg"
HOOK_SRC=".githooks/commit-msg-hook.sh"

if [ -d ".git" ]; then
    echo "Installing Git hooks..."
    cp "$HOOK_SRC" "$HOOK_DEST"
    chmod +x "$HOOK_DEST"
    echo "Hooks installed successfully."
else
    echo "Warning: .git directory not found. Skipping hook installation."
fi


BUILD_DIR="build"
CONFIG="${1:-Debug}"

cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$CONFIG" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build "$BUILD_DIR" --config "$CONFIG" --parallel "$(nproc 2>/dev/null || sysctl -n hw.logicalcpu)"

ln -sf "$BUILD_DIR/compile_commands.json" . 2>/dev/null || true

echo "Build successful. Running..."
"$BUILD_DIR/ForwardRenderer"
