@echo off
cd /d "%~dp0"
set "EXE=%CD%\build\native\Release\OllamaHud.exe"
if not exist "%EXE%" goto build
for /d %%V in ("C:\Qt\*") do for /d %%Q in ("%%~fV\msvc*_64") do if exist "%%~fQ\bin\Qt6Core.dll" set "PATH=%%~fQ\bin;%PATH%"
set "PATH=%CD%\build\native\Release;%PATH%"
"%EXE%" %*
exit /b %ERRORLEVEL%

:build
powershell -NoProfile -ExecutionPolicy Bypass -File "%CD%\scripts\build_native.ps1" -Run -- %*
exit /b %ERRORLEVEL%
