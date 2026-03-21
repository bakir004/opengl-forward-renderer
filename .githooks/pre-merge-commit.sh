#!/usr/bin/env bash

readonly HOOK_NAME="pre-merge-commit"

if [[ -t 1 ]]; then
    readonly RED=$'\033[31m'
    readonly YELLOW=$'\033[33m'
    readonly RESET=$'\033[0m'
else
    readonly RED=""
    readonly YELLOW=""
    readonly RESET=""
fi

error() {
    printf "\n${RED}[%s] ERROR:${RESET} %s\n" "${HOOK_NAME}" "$1"
}

info() {
    printf "[%s] %s\n" "${HOOK_NAME}" "$1"
}

if [[ "${BYPASS_GITFLOW_HOOKS:-0}" == "1" ]] || [[ "$(git config --bool --get hooks.gitflow.enabled 2>/dev/null)" == "false" ]]; then
    exit 0
fi

error "Merge commits are forbidden in this repository workflow."
info "Use a feature branch + pull request instead of running ${YELLOW}git merge${RESET} locally."
info "If you need updates from develop, use ${YELLOW}git rebase develop${RESET} on your feature branch."
exit 1
