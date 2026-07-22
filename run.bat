@echo off
cd /d "%~dp0"
if /I "%~1"=="verify" goto verify
if /I "%~1"=="--verify" goto verify
powershell -NoProfile -ExecutionPolicy Bypass -File "%CD%\scripts\build_native.ps1" -Run
exit /b %ERRORLEVEL%

:verify
powershell -NoProfile -ExecutionPolicy Bypass -File "%CD%\scripts\build_native.ps1" -Verify
exit /b %ERRORLEVEL%
