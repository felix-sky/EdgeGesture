<#
.SYNOPSIS
    SurfaceGesture / EdgeGesture Unified Build Script for Windows (x64 and ARM64).

.DESCRIPTION
    Configures, builds, and tests EdgeGesture and all its plugins with MSVC + Ninja + Qt 6.

.PARAMETER Arch
    Target architecture: 'x64' (default) or 'arm64'.

.PARAMETER Config
    Build configuration: 'Debug' (default for x64) or 'Release' (default for arm64).

.PARAMETER Target
    CMake target to build (e.g. 'SettingsUI', 'stagemanagerplugin', 'notes_tests', 'all').

.PARAMETER Test
    Run unit tests (ctest) after successful build (x64 only).

.PARAMETER Clean
    Wipe the target build folder before building.

.EXAMPLE
    .\build.ps1 -Arch x64 -Config Debug
    .\build.ps1 -Arch arm64 -Config Release
    .\build.ps1 -Test
    .\build.ps1 -Clean -Arch x64
#>

[CmdletBinding()]
param(
    [ValidateSet("x64", "arm64")]
    [string]$Arch = "x64",

    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Config = "",

    [string]$Target = "",

    [switch]$Test,
    [switch]$Clean
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
Set-Location $ScriptDir

# Default config if not specified
if ([string]::IsNullOrEmpty($Config)) {
    if ($Arch -eq "arm64") {
        $Config = "Release"
    } else {
        $Config = "Debug"
    }
}

$DisplayTarget = if ([string]::IsNullOrEmpty($Target)) { "SettingsUI + GestureEngine" } else { $Target }
Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host " EdgeGesture Build System" -ForegroundColor Cyan
Write-Host " Architecture: $Arch | Configuration: $Config | Target: $DisplayTarget" -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

# 1. Locate Visual Studio vcvarsall.bat
$VS_VCVARS_PATHS = @(
    "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat",
    "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat",
    "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat",
    "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat",
    "C:\Program Files (x86)\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
)

$VcvarsBat = $null
foreach ($path in $VS_VCVARS_PATHS) {
    if (Test-Path $path) {
        $VcvarsBat = $path
        break
    }
}

if (-not $VcvarsBat) {
    # Fallback to vswhere
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vsInstall = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($vsInstall) {
            $candidate = Join-Path $vsInstall "VC\Auxiliary\Build\vcvarsall.bat"
            if (Test-Path $candidate) {
                $VcvarsBat = $candidate
            }
        }
    }
}

if (-not $VcvarsBat) {
    Write-Error "Could not locate Visual Studio vcvarsall.bat! Please ensure MSVC C++ toolset is installed."
    exit 1
}
Write-Host "[+] Using MSVC environment: $VcvarsBat" -ForegroundColor Green

# 2. Locate Qt 6
$QtBaseDir = "C:\Qt"
$QtVersion = "6.10.1"

# Find highest Qt 6 version available if 6.10.1 is not present
if (-not (Test-Path (Join-Path $QtBaseDir $QtVersion))) {
    $qtDirs = Get-ChildItem -Path $QtBaseDir -Directory -Filter "6.*" -ErrorAction SilentlyContinue | Sort-Object Name -Descending
    if ($qtDirs.Count -gt 0) {
        $QtVersion = $qtDirs[0].Name
    }
}

$QtHostPrefix = "$QtBaseDir\$QtVersion\msvc2022_64"
if ($Arch -eq "arm64") {
    $QtTargetPrefix = "$QtBaseDir\$QtVersion\msvc2022_arm64"
    $VcArch = "x64_arm64"
} else {
    $QtTargetPrefix = $QtHostPrefix
    $VcArch = "x64"
}

if (-not (Test-Path $QtTargetPrefix)) {
    Write-Error "Target Qt directory not found: $QtTargetPrefix"
    exit 1
}
Write-Host "[+] Target Qt prefix: $QtTargetPrefix" -ForegroundColor Green

# 3. Locate Perl
$PerlPath = "C:\Program Files\Git\usr\bin\perl.exe"
if (-not (Test-Path $PerlPath)) {
    $perlCmd = Get-Command perl -ErrorAction SilentlyContinue
    if ($perlCmd) {
        $PerlPath = $perlCmd.Source
    } else {
        $PerlPath = "perl"
    }
}

# 4. Set unified build directory
$BuildDir = Join-Path $ScriptDir "build\$Arch-$($Config.ToLower())"

if ($Clean -and (Test-Path $BuildDir)) {
    Write-Host "[*] Cleaning build directory: $BuildDir" -ForegroundColor Yellow
    Remove-Item -Recurse -Force $BuildDir
}

if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
}

# If building ARM64, ensure host x64 indexer is built first
$HostX64Indexer = $null
if ($Arch -eq "arm64") {
    $HostX64Bin = Join-Path $ScriptDir "build\x64-$($Config.ToLower())\bin\katehighlightingindexer.exe"
    $HostX64BinDebug = Join-Path $ScriptDir "build\x64-debug\bin\katehighlightingindexer.exe"
    $HostX64BinRelease = Join-Path $ScriptDir "build\x64-release\bin\katehighlightingindexer.exe"

    if (Test-Path $HostX64Bin) {
        $HostX64Indexer = $HostX64Bin
    } elseif (Test-Path $HostX64BinDebug) {
        $HostX64Indexer = $HostX64BinDebug
    } elseif (Test-Path $HostX64BinRelease) {
        $HostX64Indexer = $HostX64BinRelease
    } else {
        Write-Host "[*] Building host tools (x64) for ARM64 code-generation..." -ForegroundColor Cyan
        & $MyInvocation.MyCommand.Definition -Arch x64 -Config Debug -Target katehighlightingindexer
        $HostX64Indexer = $HostX64BinDebug
    }
}

# 5. Execute CMake Configure & Build via cmd.exe calling vcvarsall
Write-Host "[*] Configuring and building target '$Target' ($Arch $Config)..." -ForegroundColor Cyan

$CMakePrefixPath = $QtTargetPrefix.Replace('\', '/')
$PerlPathForward = $PerlPath.Replace('\', '/')
$QtHostPrefixForward = $QtHostPrefix.Replace('\', '/')

$CMakeArgs = "-B `"$BuildDir`" -S . -G Ninja -DCMAKE_BUILD_TYPE=$Config -DPERL_EXECUTABLE=`"$PerlPathForward`" -DCMAKE_PREFIX_PATH=`"$CMakePrefixPath`""
if ($Arch -eq "arm64") {
    $CMakeArgs += " -DQT_HOST_PATH=`"$QtHostPrefixForward`""
    if ($HostX64Indexer) {
        $HostIndexerForward = $HostX64Indexer.Replace('\', '/')
        $CMakeArgs += " -DKATEHIGHLIGHTINGINDEXER_EXECUTABLE=`"$HostIndexerForward`""
    }
}

$BuildCmd = "cmake --build `"$BuildDir`""
if ($Test) {
    $BuildCmd += " --target SettingsUI GestureEngine notes_tests stage_manager_tests"
} elseif ([string]::IsNullOrEmpty($Target) -or $Target -eq "default") {
    $BuildCmd += " --target SettingsUI GestureEngine"
} elseif ($Target -ne "all") {
    $BuildCmd += " --target $Target"
}

$BatchScript = @"
@echo off
set "PATH=$QtHostPrefix\bin;C:\Program Files\Git\usr\bin;%PATH%"
call "$VcvarsBat" $VcArch
if errorlevel 1 exit /b 1

echo [*] Running CMake configure...
cmake $CMakeArgs
if errorlevel 1 exit /b 1

echo [*] Running CMake build...
$BuildCmd
if errorlevel 1 exit /b 1
"@

$TempBat = Join-Path $env:TEMP "eg_build_step_$([guid]::NewGuid().ToString('N')).bat"
Set-Content -Path $TempBat -Value $BatchScript -Encoding ASCII

try {
    & cmd.exe /c $TempBat
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Build failed with exit code $LASTEXITCODE"
        exit $LASTEXITCODE
    }
} finally {
    Remove-Item -Force $TempBat -ErrorAction SilentlyContinue
}

# 6. Copy Qt runtime DLLs for direct execution / debugging (x64 only)
if ($Arch -eq "x64") {
    $TargetBinDir = Join-Path $BuildDir "bin"
    if (Test-Path $TargetBinDir) {
        Copy-Item -Path "$QtHostPrefix\bin\Qt6Cored.dll", "$QtHostPrefix\bin\Qt6Guid.dll", "$QtHostPrefix\bin\Qt6Qmld.dll", "$QtHostPrefix\bin\Qt6Quickd.dll", "$QtHostPrefix\bin\Qt6Widgetsd.dll", "$QtHostPrefix\bin\icu*.dll" -Destination $TargetBinDir -Force -ErrorAction SilentlyContinue
    }
}

Write-Host "==========================================================" -ForegroundColor Green
Write-Host " [SUCCESS] Build completed successfully!" -ForegroundColor Green
Write-Host " Output Directory: $BuildDir" -ForegroundColor Green
Write-Host "==========================================================" -ForegroundColor Green

# 7. Run Tests if requested
if ($Test) {
    if ($Arch -eq "arm64") {
        Write-Host "[!] Skipping tests on ARM64 cross-compilation." -ForegroundColor Yellow
    } else {
        Write-Host "[*] Running CTest test suite..." -ForegroundColor Cyan
        $TestBatch = @"
@echo off
set "PATH=$QtHostPrefix\bin;%PATH%"
ctest --test-dir "$BuildDir" --output-on-failure
"@
        $TempTestBat = Join-Path $env:TEMP "eg_test_step_$([guid]::NewGuid().ToString('N')).bat"
        Set-Content -Path $TempTestBat -Value $TestBatch -Encoding ASCII
        try {
            & cmd.exe /c $TempTestBat
            if ($LASTEXITCODE -ne 0) {
                Write-Error "Tests failed with exit code $LASTEXITCODE"
                exit $LASTEXITCODE
            }
        } finally {
            Remove-Item -Force $TempTestBat -ErrorAction SilentlyContinue
        }
    }
}
