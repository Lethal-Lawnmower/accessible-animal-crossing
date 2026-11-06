# Build the Windows MSVC preset, including the Prism speech library.
#
# Usage (from the repository root):
#   powershell -ExecutionPolicy Bypass -File tools\build-windows-msvc.ps1 [-Preset <name>]
# or simply: tools\build-windows-msvc.bat [preset]
#
# What it picks, and why:
# - Compiler: the newest Visual Studio (or Build Tools) install that has both the
#   x64 C++ tools and ATL. Prism's Windows screen reader backends need ATL.
# - Game CMake: the CMake bundled with that same install. A different CMake
#   earlier on PATH once broke the resource compiler step (RC1107).
# - Prism CMake: the newest CMake 4.1+ found in any Visual Studio install or on
#   PATH. Prism uses C23, which older CMake cannot enable for MSVC.

param(
    [string]$Preset = "windows-msvc-relwithdebinfo"
)

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    throw "vswhere.exe not found. Install Visual Studio or its Build Tools with the C++ workload."
}

function Get-CMakeVersion([string]$exe) {
    try {
        $line = (& $exe --version 2>$null | Select-Object -First 1)
        if ($line -match "cmake version (\d+)\.(\d+)\.(\d+)") {
            return [version]"$($Matches[1]).$($Matches[2]).$($Matches[3])"
        }
    } catch {}
    return $null
}

function Get-BundledCMake([string]$installPath) {
    $exe = Join-Path $installPath "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    if (Test-Path $exe) { return $exe }
    return $null
}

# Compiler install: newest one with x64 tools and ATL.
$compilerInstall = & $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 Microsoft.VisualStudio.Component.VC.ATL `
    -property installationPath
if (-not $compilerInstall) {
    throw ("No Visual Studio install with C++ x64 tools and ATL was found. " +
           "In the Visual Studio Installer, add 'C++ ATL for latest build tools (x86 and x64)'.")
}
$gameCMake = Get-BundledCMake $compilerInstall
if (-not $gameCMake) {
    throw "The Visual Studio install at $compilerInstall has no bundled CMake. Add the 'C++ CMake tools for Windows' component."
}

# Prism CMake: newest 4.1+ from any install or PATH.
$candidates = @()
foreach ($path in (& $vswhere -all -products * -property installationPath)) {
    $exe = Get-BundledCMake $path
    if ($exe) { $candidates += $exe }
}
$onPath = Get-Command cmake -ErrorAction SilentlyContinue
if ($onPath) { $candidates += $onPath.Source }
$prismCMake = $null
$prismVersion = $null
foreach ($exe in ($candidates | Select-Object -Unique)) {
    $v = Get-CMakeVersion $exe
    if ($v -and $v -ge [version]"4.1.0" -and (-not $prismVersion -or $v -gt $prismVersion)) {
        $prismCMake = $exe
        $prismVersion = $v
    }
}
if (-not $prismCMake) {
    throw ("Prism needs CMake 4.1 or newer and none was found. Install Visual Studio 2026 Build Tools, " +
           "or CMake 4 from cmake.org, then run this script again.")
}

Write-Host "Compiler:   $compilerInstall"
Write-Host "Game CMake: $gameCMake ($(Get-CMakeVersion $gameCMake))"
Write-Host "Prism CMake: $prismCMake ($prismVersion)"

# Load the x64 compiler environment into this PowerShell session.
$vcvars = Join-Path $compilerInstall "VC\Auxiliary\Build\vcvars64.bat"
cmd /c "`"$vcvars`" >nul && set" | ForEach-Object {
    if ($_ -match "^([^=]+)=(.*)$") { Set-Item -Path "env:$($Matches[1])" -Value $Matches[2] }
}
$bundledNinja = Join-Path $compilerInstall "Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja"
$env:PATH = "$(Split-Path $gameCMake);$bundledNinja;$env:PATH"

Push-Location $repo
try {
    & $gameCMake --preset $Preset "-DACCESS_PRISM_CMAKE_COMMAND=$prismCMake"
    if ($LASTEXITCODE -ne 0) { exit 2 }
    & $gameCMake --build --preset $Preset
    if ($LASTEXITCODE -ne 0) { exit 3 }
    Write-Host "Build finished: build\$Preset\bin\Nookery.exe"
} finally {
    Pop-Location
}
