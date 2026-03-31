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

set "GENERATOR="
if defined CMAKE_GENERATOR (
    set "GENERATOR=%CMAKE_GENERATOR%"
) else (
    where ninja >nul 2>&1
    if not errorlevel 1 set "GENERATOR=Ninja"

    if not defined GENERATOR (
        where g++ >nul 2>&1
        if not errorlevel 1 (
            where mingw32-make >nul 2>&1
            if not errorlevel 1 set "GENERATOR=MinGW Makefiles"
        )
    )

    if not defined GENERATOR (
        where cl >nul 2>&1
        if not errorlevel 1 (
            where nmake >nul 2>&1
            if not errorlevel 1 set "GENERATOR=NMake Makefiles"
        )
    )
)

if not defined GENERATOR (
    echo Error: No supported C/C++ toolchain detected.
    echo Install one of the following options and re-run build.bat:
    echo   1^) Visual Studio Build Tools ^(C++ workload^) + CMake/Ninja
    echo   2^) MSYS2/MinGW-w64 with g++ and mingw32-make
    exit /b 1
)

if exist "%BUILD_DIR%\CMakeCache.txt" (
    findstr /B /C:"CMAKE_GENERATOR:INTERNAL=" "%BUILD_DIR%\CMakeCache.txt" >nul 2>&1
    if not errorlevel 1 (
        findstr /B /C:"CMAKE_GENERATOR:INTERNAL=%GENERATOR%" "%BUILD_DIR%\CMakeCache.txt" >nul 2>&1
        if errorlevel 1 (
            echo Existing CMake cache uses a different generator. Recreating %BUILD_DIR%...
            rmdir /S /Q "%BUILD_DIR%"
        )
    )
)

echo Configuring with generator: %GENERATOR%

cmake -S . -B %BUILD_DIR% -G "%GENERATOR%" -DCMAKE_BUILD_TYPE=%CONFIG% -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

cmake --build %BUILD_DIR% --config %CONFIG% --parallel %NUMBER_OF_PROCESSORS%
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

if exist "%BUILD_DIR%\compile_commands.json" (
    mklink /H compile_commands.json "%BUILD_DIR%\compile_commands.json" >nul 2>&1
)

echo Build successful. Running...
set "RUN_EXE="

if exist "%BUILD_DIR%\test-app\%CONFIG%\TestApp.exe" set "RUN_EXE=%BUILD_DIR%\test-app\%CONFIG%\TestApp.exe"
if not defined RUN_EXE if exist "%BUILD_DIR%\test-app\TestApp.exe" set "RUN_EXE=%BUILD_DIR%\test-app\TestApp.exe"
if not defined RUN_EXE if exist "%BUILD_DIR%\%CONFIG%\TestApp.exe" set "RUN_EXE=%BUILD_DIR%\%CONFIG%\TestApp.exe"
if not defined RUN_EXE if exist "%BUILD_DIR%\TestApp.exe" set "RUN_EXE=%BUILD_DIR%\TestApp.exe"

if not defined RUN_EXE (
    for /R "%BUILD_DIR%" %%F in (TestApp.exe) do (
        set "RUN_EXE=%%~fF"
        goto :run_app
    )
)

:run_app

if defined RUN_EXE (
    "%RUN_EXE%"
) else (
    echo Error: Built executable not found. Expected TestApp.exe in %BUILD_DIR% output directories.
    exit /b 1
)
