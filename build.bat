@echo off
setlocal

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