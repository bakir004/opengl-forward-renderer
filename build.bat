@echo off
setlocal

@echo off
SET HOOK_DEST=.git\hooks\commit-msg
SET HOOK_SRC=.githooks\commit-msg-hook.ps1

IF EXIST .git (
    echo Installing Git hooks for Windows...
    :: Create a wrapper that calls PowerShell
    echo @echo off > %HOOK_DEST%
    echo powershell -ExecutionPolicy Bypass -File "%CD%\%HOOK_SRC%" "%%1" >> %HOOK_DEST%
    echo Hooks installed successfully.
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
