@echo off
rem Build the Windows MSVC preset from any command prompt.
rem All the logic lives in build-windows-msvc.ps1; see the notes there.
rem
rem Usage: tools\build-windows-msvc.bat [preset]
rem Default preset: windows-msvc-relwithdebinfo

setlocal
set "PRESET=%~1"
if "%PRESET%"=="" set "PRESET=windows-msvc-relwithdebinfo"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build-windows-msvc.ps1" -Preset "%PRESET%"
exit /b %errorlevel%
