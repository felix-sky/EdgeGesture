@echo off
setlocal enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"
cd /d "%SCRIPT_DIR%"

if "%1"=="x64" (
    powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%build.ps1" -Arch x64 -Config Debug
    exit /b %ERRORLEVEL%
)

if "%1"=="x64-release" (
    powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%build.ps1" -Arch x64 -Config Release
    exit /b %ERRORLEVEL%
)

if "%1"=="arm64" (
    powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%build.ps1" -Arch arm64 -Config Release
    exit /b %ERRORLEVEL%
)

if "%1"=="test" (
    powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%build.ps1" -Arch x64 -Config Debug -Test
    exit /b %ERRORLEVEL%
)

if "%1"=="clean" (
    echo [*] Cleaning all build directories...
    if exist "%SCRIPT_DIR%build" rd /s /q "%SCRIPT_DIR%build"
    if exist "%SCRIPT_DIR%build_msvc" rd /s /q "%SCRIPT_DIR%build_msvc"
    if exist "%SCRIPT_DIR%build_vs" rd /s /q "%SCRIPT_DIR%build_vs"
    echo [+] Clean complete.
    exit /b 0
)

:: Interactive Menu if no arguments passed
echo ========================================================
echo   SurfaceGesture / EdgeGesture Unified Build Menu
echo ========================================================
echo   [1] Build x64 (Debug)
echo   [2] Build x64 (Release)
echo   [3] Build ARM64 (Release)
echo   [4] Build x64 and Run Unit Tests
echo   [5] Clean all build folders (build, build_msvc, build_vs)
echo   [0] Exit
echo ========================================================
set /p CHOICE="Select an option [1-5, 0]: "

if "%CHOICE%"=="1" (
    powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%build.ps1" -Arch x64 -Config Debug
) else if "%CHOICE%"=="2" (
    powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%build.ps1" -Arch x64 -Config Release
) else if "%CHOICE%"=="3" (
    powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%build.ps1" -Arch arm64 -Config Release
) else if "%CHOICE%"=="4" (
    powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%build.ps1" -Arch x64 -Config Debug -Test
) else if "%CHOICE%"=="5" (
    echo [*] Cleaning all build directories...
    if exist "%SCRIPT_DIR%build" rd /s /q "%SCRIPT_DIR%build"
    if exist "%SCRIPT_DIR%build_msvc" rd /s /q "%SCRIPT_DIR%build_msvc"
    if exist "%SCRIPT_DIR%build_vs" rd /s /q "%SCRIPT_DIR%build_vs"
    echo [+] Clean complete.
) else (
    echo Exiting.
)

pause
