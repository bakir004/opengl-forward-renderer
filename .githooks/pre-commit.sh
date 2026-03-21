#!/bin/bash

current_branch=$(git rev-parse --abbrev-ref HEAD)

protected_pattern="^(main|develop)$"

if [[ $current_branch =~ $protected_pattern ]]; then
    echo -e "\n\e[31m[ERROR] Direct commits to '$current_branch' are forbidden!\e[0m"
    echo -e "This branch is protected. You must use a Feature Branch and a Pull Request."
    echo -e "-------------------------------------------------------"
    echo -e "\e[1mQuick Fix:\e[0m"
    echo -e "  1. \e[33mgit stash\e[0m"
    echo -e "  2. \e[33mgit checkout -b feature/your-task-name\e[0m"
    echo -e "  3. \e[33mgit stash pop\e[0m"
    echo -e "-------------------------------------------------------"
    exit 1
fi
