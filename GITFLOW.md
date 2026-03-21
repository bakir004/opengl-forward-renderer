# Git Workflow Guide

This repository uses enforced Git hooks to keep history clean and consistent.

## Branch Strategy

- Protected branches: `main`, `develop`
- Do not commit directly to protected branches.
- Create topic branches from `develop` using:
  - `feature/<name>`
  - `bugfix/<name>`
  - `hotfix/<name>`
  - `docs/<name>`
  - `refactor/<name>`
  - `test/<name>`

Examples:

- `feature/frustum-culling`
- `bugfix/shader-uniform-crash`

## Commit Message Format

Commits must follow Conventional Commit style:

`type(scope): description`

Allowed types:

- `feat`
- `fix`
- `docs`
- `style`
- `refactor`
- `perf`
- `test`
- `build`
- `ci`
- `chore`
- `hotfix`

Examples:

- `feat(renderer): add frustum culling`
- `fix(camera): clamp pitch to avoid gimbal flip`

## Forbidden Operations (Enforced)

- Direct commits on `main` or `develop`
- Merge commits (`git merge`) in local workflow
- Rebasing while on `main` or `develop`
- Non-compliant branch names
- Non-compliant commit messages

## Daily Flow

1. Update local `develop`
   - `git switch develop`
   - `git pull`
2. Create a working branch
   - `git switch -c feature/your-task-name`
3. Work and commit
   - `git add .`
   - `git commit -m "feat(scope): description"`
4. Push and open PR to `develop`
   - `git push -u origin HEAD`
5. Merge through PR (no local merge commits)

## If You Accidentally Committed on `develop` or `main`

- `git stash push -u`
- `git switch -c feature/your-task-name`
- `git stash pop`

## Hook Setup

Hooks are installed by build scripts:

- Linux/macOS/Git Bash: `./build.sh`
- Windows CMD/PowerShell: `build.bat`

They copy `.githooks/*.sh` to `.git/hooks/*` (without `.sh` extension).

## Temporary Bypass (Edge Cases)

Use these only when manual intervention is required.

Toggle for this repo until re-enabled:

- Disable: `git config --local hooks.gitflow.enabled false`
- Re-enable: `git config --local hooks.gitflow.enabled true`
- Check: `git config --get --bool hooks.gitflow.enabled`
