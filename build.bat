@echo off
setlocal

REM --- Git Hooks Setup ---
SET HOOK_SRC_DIR=.githooks
SET HOOK_DEST_DIR=.git\hooks

IF EXIST .git (
    IF EXIST "%HOOK_SRC_DIR%\*.sh" (
        echo Installing Git hooks from %HOOK_SRC_DIR%...
        for %%F in ("%HOOK_SRC_DIR%\*.sh") do (
            copy /Y "%%~fF" "%HOOK_DEST_DIR%\%%~nF" >nul
            echo   - Installed %%~nF
        )
        echo Git hooks installation complete.
    ) ELSE (
        echo Warning: no .sh hooks found in %HOOK_SRC_DIR%. Skipping hook installation.
    )
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
