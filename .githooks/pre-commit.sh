#!/usr/bin/env bash

readonly HOOK_NAME="pre-commit"
readonly RED='\033[31m'
readonly YELLOW='\033[33m'
readonly RESET='\033[0m'

error() {
    printf "\n${RED}[%s] ERROR:${RESET} %s\n" "${HOOK_NAME}" "$1"
}

info() {
    printf "[%s] %s\n" "${HOOK_NAME}" "$1"
}

# Works in Linux/macOS and Windows Git Bash.
current_branch="$(git symbolic-ref --quiet --short HEAD 2>/dev/null || git rev-parse --abbrev-ref HEAD)"
protected_pattern='^(main|develop)$'

if [[ "${current_branch}" =~ ${protected_pattern} ]]; then
    error "Direct commits to '${current_branch}' are forbidden."
    info "This branch is protected. Use a feature branch and open a pull request."
    info "Quick fix:"
    info "  1. ${YELLOW}git stash push -u${RESET}"
    info "  2. ${YELLOW}git switch -c feature/your-task-name${RESET} (or: git checkout -b feature/your-task-name)"
    info "  3. ${YELLOW}git stash pop${RESET}"
    exit 1
fi
