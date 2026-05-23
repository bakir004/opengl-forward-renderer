@echo off
start "TestApp" cmd /c "build\test-app\Debug\TestApp.exe > app_log.txt 2>&1"
timeout /t 3
taskkill /IM TestApp.exe /F
