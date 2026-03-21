@echo off
setlocal

REM --- Git Hooks Setup ---
SET HOOK_SRC_DIR=.githooks
SET HOOK_DEST_DIR=.git\hooks

IF EXIST .git (
    IF EXIST "%HOOK_SRC_DIR%\pre-commit.sh" (
        copy /Y "%HOOK_SRC_DIR%\pre-commit.sh" "%HOOK_DEST_DIR%\pre-commit" >nul
        echo Installed pre-commit
    ) ELSE (
        echo Warning: %HOOK_SRC_DIR%\pre-commit.sh not found. Skipping pre-commit.
    )

    IF EXIST "%HOOK_SRC_DIR%\commit-msg.sh" (
        copy /Y "%HOOK_SRC_DIR%\commit-msg.sh" "%HOOK_DEST_DIR%\commit-msg" >nul
        echo Installed commit-msg
    ) ELSE (
        echo Warning: %HOOK_SRC_DIR%\commit-msg.sh not found. Skipping commit-msg.
    )

    echo Git hooks installation complete.
) ELSE (
    echo Warning: .git directory not found. Skipping hook installation.
)

set BUILD_DIR=build
set CONFIG=%1
if "%CONFIG%"=="" set CONFIG=Debug

cmake -B %BUILD_DIR% -DCMAKE_BUILD_TYPE=%CONFIG% -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

cmake --build %BUILD_DIR% --config %CONFIG% --parallel %NUMBER_OF_PROCESSORS%
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

if exist "%BUILD_DIR%\compile_commands.json" (
    mklink /H compile_commands.json "%BUILD_DIR%\compile_commands.json" >nul 2>&1
)

echo Build successful. Running...
"%BUILD_DIR%\%CONFIG%\ForwardRenderer.exe"
