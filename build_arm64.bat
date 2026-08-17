@echo off
set "SCRIPT_DIR=%~dp0"
call "%SCRIPT_DIR%build.bat" arm64
pause
