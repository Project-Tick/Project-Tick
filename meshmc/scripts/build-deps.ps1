# SPDX-FileCopyrightText: 2026 Project Tick
# SPDX-License-Identifier: GPL-3.0-or-later
#
# build-deps.ps1 — Build and install all MeshMC monorepo dependencies, then
#                   configure MeshMC itself.  (Windows — PowerShell)
#
# Usage:
#   .\scripts\build-deps.ps1 [-Configure] [-Build] [-Clean] [-Jobs N]
#
# Parameters:
#   -Configure    Also configure MeshMC after installing dependencies
#   -Build        Also build MeshMC (implies -Configure)
#   -Clean        Remove existing build directories before building
#   -Jobs N       Parallel build jobs (default: number of processors)
#
# Environment variables:
#   MONOREPO_ROOT     Override monorepo root (default: auto-detected)
#   INSTALL_PREFIX    Override install prefix
#   BUILD_TYPE        Override build type (default: Release)
#   VCPKG_ROOT        Path to vcpkg (required for MSVC builds)
#   CMAKE_GENERATOR   Override generator (default: Ninja)

[CmdletBinding()]
param(
    [switch]$Configure,
    [switch]$Build,
    [switch]$Clean,
    [int]$Jobs = 0
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

##############################################################################
# Helpers
##############################################################################

function Write-Log   { param([string]$Msg) Write-Host "[build-deps] $Msg" -ForegroundColor Green }
function Write-Warn  { param([string]$Msg) Write-Warning "[build-deps] $Msg" }
function Write-Err   { param([string]$Msg) Write-Host "[build-deps] $Msg" -ForegroundColor Red; exit 1 }

function Invoke-Cmd {
    $cmdLine = $args -join ' '
    Write-Host "  > $cmdLine" -ForegroundColor DarkGray
    & $args[0] $args[1..($args.Length - 1)]
    if ($LASTEXITCODE -ne 0) {
        Write-Err "Command failed (exit $LASTEXITCODE): $cmdLine"
    }
}

##############################################################################
# Defaults
##############################################################################

$ScriptDir    = Split-Path -Parent $MyInvocation.MyCommand.Definition
$MonorepoRoot = if ($env:MONOREPO_ROOT) { $env:MONOREPO_ROOT } else { (Resolve-Path "$ScriptDir\..\..").Path }
$MeshMCDir    = Join-Path $MonorepoRoot 'meshmc'
$BuildType    = if ($env:BUILD_TYPE)    { $env:BUILD_TYPE }       else { 'Release' }
$Generator    = if ($env:CMAKE_GENERATOR) { $env:CMAKE_GENERATOR } else { 'Ninja' }

if ($Jobs -le 0) {
    $Jobs = $env:NUMBER_OF_PROCESSORS
    if (-not $Jobs) { $Jobs = 4 }
}

if ($Build) { $Configure = $true }

##############################################################################
# Platform detection — MSVC vs MinGW
##############################################################################

# If MINGW_PREFIX is set we're inside MSYS2 — but that means the user should
# use build-deps.sh instead, so this script assumes MSVC.

$Platform = 'windows-msvc'
Write-Log "Platform: $Platform"

##############################################################################
# CMake flags
##############################################################################

$VcpkgRoot = $env:VCPKG_ROOT
if (-not $VcpkgRoot) {
    Write-Err 'VCPKG_ROOT is not set. Please install vcpkg and set VCPKG_ROOT.'
}

$InstallPrefix = if ($env:INSTALL_PREFIX) {
    $env:INSTALL_PREFIX
} else {
    Join-Path $MonorepoRoot 'meshmc-deps'
}

$CMakeCommon = @(
    "-DCMAKE_INSTALL_PREFIX=$InstallPrefix"
    "-DCMAKE_BUILD_TYPE=$BuildType"
    "-DCMAKE_TOOLCHAIN_FILE=$VcpkgRoot\scripts\buildsystems\vcpkg.cmake"
)

Write-Log "Install prefix: $InstallPrefix"
Write-Log "Build type:     $BuildType"
Write-Log "Generator:      $Generator"
Write-Log "Jobs:           $Jobs"
Write-Log "vcpkg:          $VcpkgRoot"

##############################################################################
# Build a single library
##############################################################################

$script:BuiltCount = 0
$TotalLibs = 15

function Install-Lib {
    param(
        [Parameter(Mandatory)][string]$Dir,
        [Parameter(ValueFromRemainingArguments)][string[]]$ExtraArgs
    )

    $script:BuiltCount++
    $Src = Join-Path $MonorepoRoot $Dir
    $Bld = Join-Path $Src 'build'

    if (-not (Test-Path $Src -PathType Container)) {
        Write-Err "Directory not found: $Src"
    }

    Write-Log "[$script:BuiltCount/$TotalLibs] Building $Dir ..."

    if ($Clean -and (Test-Path $Bld)) {
        Remove-Item $Bld -Recurse -Force
    }

    $configArgs = @('-S', $Src, '-B', $Bld) + $CMakeCommon
    if ($ExtraArgs) { $configArgs += $ExtraArgs }
    $configArgs += @('-G', $Generator)
    Invoke-Cmd cmake @configArgs

    Invoke-Cmd cmake --build $Bld --parallel $Jobs

    Invoke-Cmd cmake --install $Bld
}

##############################################################################
# Build all dependencies (in correct order)
##############################################################################

function Build-Deps {
    Write-Log 'Building all MeshMC dependencies...'
    Write-Host ''

    # Level 1: No monorepo dependencies
    Install-Lib neozip        '-DZLIB_COMPAT=OFF' '-DWITH_GTEST=OFF'
    Install-Lib cmark
    Install-Lib tomlplusplus

    # Level 2: Depends on Level 1
    Install-Lib libnbtplusplus

    # Level 3: Qt-dependent, no monorepo inter-deps
    Install-Lib optional-bare
    Install-Lib xz-embedded
    Install-Lib systeminfo
    Install-Lib rainbow
    Install-Lib iconfix
    Install-Lib LocalPeer
    Install-Lib classparser
    Install-Lib katabasis

    # Level 4: Depends on Level 3
    Install-Lib ganalytics

    # Java jars
    Install-Lib javacheck
    Install-Lib javalauncher

    Write-Host ''
    Write-Log 'All dependencies built and installed successfully!'
}

##############################################################################
# Configure MeshMC
##############################################################################

function Configure-MeshMC {
    Write-Log 'Configuring MeshMC...'

    $Preset = 'windows_msvc'

    # Ensure the dependency prefix is on CMAKE_PREFIX_PATH
    if ($env:CMAKE_PREFIX_PATH) {
        $env:CMAKE_PREFIX_PATH = "$InstallPrefix;$env:CMAKE_PREFIX_PATH"
    } else {
        $env:CMAKE_PREFIX_PATH = $InstallPrefix
    }

    Invoke-Cmd cmake --preset $Preset -S $MeshMCDir

    Write-Log "MeshMC configured with preset: $Preset"
}

##############################################################################
# Build MeshMC
##############################################################################

function Build-MeshMC {
    Write-Log 'Building MeshMC...'

    $Preset = 'windows_msvc'
    Invoke-Cmd cmake --build --preset $Preset --config $BuildType --parallel $Jobs

    Write-Log 'MeshMC built successfully!'
}

##############################################################################
# Main
##############################################################################

Build-Deps

if ($Configure) {
    Write-Host ''
    Configure-MeshMC
}

if ($Build) {
    Write-Host ''
    Build-MeshMC
}

Write-Host ''
Write-Log 'Done!'
