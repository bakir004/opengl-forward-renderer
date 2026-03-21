#!/bin/bash

commit_msg=$(cat "$1")

types="feat|fix|docs|style|refactor|perf|test|build|ci|chore|hotfix"

pattern="^($types)(\([a-z0-9-]+\))?: .+"

if [[ ! $commit_msg =~ $pattern ]]; then
    echo -e "\n\e[31m[ERROR] Invalid commit message format!\e[0m\n"
    echo -e "Expected format: \e[32mtype(scope): description\e[0m"
    echo -e "Example:         \e[32mfeat(renderer): add shadow mapping\e[0m\n"
    echo -e "\e[1mAvailable keywords (types):\e[0m"
    echo "$types" | tr '|' ' ' | sed 's/^//'
    
    echo -e "\nNote: Scope is optional but must be in parentheses if used."
    exit 1
fi
