@echo off
set "SOURCE=%~dp0src_ui\plugin"

echo Copying plugins to build\x64-debug\plugin...
if exist "%~dp0build\x64-debug" xcopy /E /I /Y "%SOURCE%" "%~dp0build\x64-debug\plugin"

echo Copying plugins to build\x64-release\plugin...
if exist "%~dp0build\x64-release" xcopy /E /I /Y "%SOURCE%" "%~dp0build\x64-release\plugin"

echo Copying plugins to build\arm64-release\plugin...
if exist "%~dp0build\arm64-release" xcopy /E /I /Y "%SOURCE%" "%~dp0build\arm64-release\plugin"

echo Done.
pause
