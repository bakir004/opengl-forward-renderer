#!/usr/bin/env bash

readonly HOOK_NAME="commit-msg"
readonly RED='\033[31m'
readonly GREEN='\033[32m'
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
branch_pattern='^(main|develop|((feature|bugfix|hotfix|docs|refactor|test)/[a-z0-9-]+))$'

if [[ ! "${current_branch}" =~ ${branch_pattern} ]]; then
    error "Invalid branch name: '${current_branch}'"
    info "Use one of: ${GREEN}main, develop, feature/, bugfix/, hotfix/, docs/, refactor/, test/${RESET}"
    info "Example: ${GREEN}feature/frustum-culling${RESET}"
    info "Rename command:"
    info "  ${YELLOW}git branch -m <category>/<description>${RESET}"
    exit 1
fi

# Read first line only, strip possible CR from CRLF files (common on Windows).
IFS= read -r commit_msg < "$1"
commit_msg="${commit_msg%$'\r'}"

types='feat|fix|docs|style|refactor|perf|test|build|ci|chore|hotfix'
msg_pattern="^(${types})(\\([a-z0-9-]+\\))?: .+"

if [[ ! "${commit_msg}" =~ ${msg_pattern} ]]; then
    error "Invalid commit message format."
    info "Expected: ${GREEN}type(scope): description${RESET}"
    info "Allowed types: ${GREEN}${types}${RESET}"
    info "Example: ${GREEN}feat(renderer): add frustum culling${RESET}"
    exit 1
fi
