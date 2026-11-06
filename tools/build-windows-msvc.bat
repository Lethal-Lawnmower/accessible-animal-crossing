@echo off
rem Build the Windows MSVC preset from any command prompt.
rem
rem Finds Visual Studio (or its Build Tools) with vswhere, loads the x64
rem compiler environment, and puts Visual Studio's own CMake and Ninja first on
rem PATH. That matters when another CMake is installed earlier on PATH: one
rem such copy produced resource-compiler (RC1107) failures in this project.
rem
rem Usage: tools\build-windows-msvc.bat [preset]
rem Default preset: windows-msvc-relwithdebinfo

setlocal
set "PRESET=%~1"
if "%PRESET%"=="" set "PRESET=windows-msvc-relwithdebinfo"

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo vswhere.exe not found. Install Visual Studio with the C++ workload.
    exit /b 1
)
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR (
    echo No Visual Studio installation with C++ x64 tools was found.
    exit /b 1
)

call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set "PATH=%VSDIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%VSDIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"

cd /d "%~dp0.."
cmake --preset %PRESET% || exit /b 2
cmake --build --preset %PRESET% || exit /b 3
echo Build finished: build\%PRESET%\bin\Nookery.exe
