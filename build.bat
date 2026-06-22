@echo off
setlocal

REM ==============================
REM CONFIGURATION
REM ==============================
set "BUILD_DIR=build"
set "CONFIG=%1"
if "%CONFIG%"=="" set "CONFIG=Release"

REM ==============================
REM GIT HOOKS (OPTIONAL SAFE STEP)
REM ==============================
if exist .git (
    if exist ".githooks" (
        echo Installing Git hooks...
        for %%F in (.githooks\*) do (
            copy /Y "%%F" ".git\hooks\%%~nxF" >nul
        )
    )
)

REM ==============================
REM CONFIGURE (LET CMAKE DECIDE GENERATOR)
REM ==============================
echo Configuring CMake (%CONFIG%)...

cmake -S . -B "%BUILD_DIR%" ^ -G "MinGW Makefiles" ^
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

if errorlevel 1 exit /b 1

REM ==============================
REM BUILD
REM ==============================
echo Building (%CONFIG%)...

cmake --build "%BUILD_DIR%" --config "%CONFIG%" --parallel

if errorlevel 1 exit /b 1

REM ==============================
REM FIND EXECUTABLE (FLEXIBLE)
REM ==============================
set "RUN_EXE="

for /R "%BUILD_DIR%" %%F in (*.exe) do (
    if /I "%%~nF"=="TestApp" (
        set "RUN_EXE=%%F"
        goto :run
    )
)

echo Error: TestApp.exe not found.
exit /b 1

:run
echo Running: %RUN_EXE%
"%RUN_EXE%"

endlocal