#!/usr/bin/env bash

readonly HOOK_NAME="pre-rebase"
readonly RED='\033[31m'
readonly YELLOW='\033[33m'
readonly RESET='\033[0m'

error() {
    printf "\n${RED}[%s] ERROR:${RESET} %s\n" "${HOOK_NAME}" "$1"
}

info() {
    printf "[%s] %s\n" "${HOOK_NAME}" "$1"
}

if [[ "${BYPASS_GITFLOW_HOOKS:-0}" == "1" ]] || [[ "$(git config --bool --get hooks.gitflow.enabled 2>/dev/null)" == "false" ]]; then
    exit 0
fi

# Works in Linux/macOS and Windows Git Bash.
current_branch="$(git symbolic-ref --quiet --short HEAD 2>/dev/null || git rev-parse --abbrev-ref HEAD)"
protected_pattern='^(main|develop)$'

if [[ "${current_branch}" =~ ${protected_pattern} ]]; then
    error "Rebasing '${current_branch}' is forbidden."
    info "Do rebase only on feature branches."
    info "Create one with ${YELLOW}git switch -c feature/your-task-name${RESET}"
    exit 1
fi
