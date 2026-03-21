#!/bin/bash

# 1. BRANCH NAME CHECK
# Get the current branch name
current_branch=$(git rev-parse --abbrev-ref HEAD)

# Allowed branch prefixes
# main and develop are exempt from the prefix rule
branch_pattern="^(main|develop|((feature|bugfix|hotfix|docs|refactor|test)\/[a-z0-9-]+))$"

if [[ ! $current_branch =~ $branch_pattern ]]; then
    echo -e "\n\e[31m[ERROR] Invalid branch name: '$current_branch'\e[0m"
    echo -e "Branches must start with a category prefix."
    echo -e "Allowed prefixes: \e[32mfeature/, bugfix/, hotfix/, docs/, refactor/, test/\e[0m"
    echo -e "Example: \e[32mfeature/frustum-culling\e[0m"
    echo -e "-------------------------------------------------------"
    exit 1
fi

# 2. COMMIT MESSAGE CHECK
commit_msg=$(cat "$1")
types="feat|fix|docs|style|refactor|perf|test|build|ci|chore|hotfix"
msg_pattern="^($types)(\([a-z0-9-]+\))?: .+"

if [[ ! $commit_msg =~ $msg_pattern ]]; then
    echo -e "\n\e[31m[ERROR] Invalid commit message format!\e[0m"
    echo -e "Expected: \e[32mtype(scope): description\e[0m"
    echo -e "Available types: $types"
    exit 1
fi
