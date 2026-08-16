@echo off
set "SOURCE=%~dp0src_ui\plugin"

echo Copying plugins to Release...
xcopy /E /I /Y "%SOURCE%" "%~dp0build\Desktop_Qt_6_10_1_MSVC2022_64bit-Release\plugin"

echo Copying plugins to Debug...
xcopy /E /I /Y "%SOURCE%" "%~dp0build\Desktop_Qt_6_10_1_MSVC2022_64bit-Debug\plugin"

echo Copying plugins to Profile...
xcopy /E /I /Y "%SOURCE%" "%~dp0build\Desktop_Qt_6_10_1_MSVC2022_64bit-Profile\plugin"

echo Done.
pause
