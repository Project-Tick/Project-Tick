@echo off
rem SPDX-License-Identifier: GPL-3.0-or-later
rem SPDX-FileCopyrightText: 2026 Project Tick
rem
rem Project Tick Bootstrap Script (Windows)
rem Checks dependencies, installs via scoop, and sets up lefthook.

setlocal EnableDelayedExpansion

cd /d "%~dp0"

echo.
echo [INFO]  Project Tick Bootstrap
echo ---------------------------------------------
echo.

rem ── Submodules ──────────────────────────────────────────────────────────────
if not exist ".git" (
    echo [ERR]  Not a git repository. Please run this script from the project root. 1>&2
    exit /b 1
)

echo [INFO]  Setting up submodules...
git submodule update --init --recursive
if errorlevel 1 (
    echo [ERR]  Submodule initialization failed. 1>&2
    exit /b 1
)
echo [ OK ]  Submodules initialized
echo.

rem ── Check for scoop ─────────────────────────────────────────────────────────
set "HAS_SCOOP=0"
where scoop >nul 2>&1 && set "HAS_SCOOP=1"

if "%HAS_SCOOP%"=="0" (
    echo [WARN]  Scoop is not installed.
    echo [INFO]  Installing Scoop...
    powershell -NoProfile -Command "Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser -Force; Invoke-RestMethod -Uri https://get.scoop.sh | Invoke-Expression"
    if errorlevel 1 (
        echo [ERR]  Scoop installation failed. Install manually from https://scoop.sh 1>&2
        exit /b 1
    )
    rem Refresh PATH
    set "PATH=%USERPROFILE%\scoop\shims;%PATH%"
    echo [ OK ]  Scoop installed
) else (
    echo [ OK ]  Scoop is installed
)
echo.

rem ── Dependency Checks ──────────────────────────────────────────────────────
echo [INFO]  Checking dependencies...
echo.

set "MISSING="

call :check_cmd npm npm
call :check_cmd Go go
call :check_cmd lefthook lefthook
call :check_cmd reuse reuse
call :check_cmd CMake cmake
call :check_cmd Git git

echo.

rem ── Install Missing Dependencies via Scoop ─────────────────────────────────
if not defined MISSING (
    echo [ OK ]  All command-line dependencies are already installed!
    goto :install_libs
)

echo [INFO]  Missing dependencies: %MISSING%
echo [INFO]  Installing via Scoop...
echo.

rem Add extras bucket for some packages
scoop bucket add extras >nul 2>&1
scoop bucket add main >nul 2>&1

echo !MISSING! | findstr /i "npm" >nul && (
    echo [INFO]  Installing Node.js ^(includes npm^)...
    scoop install nodejs
)

echo !MISSING! | findstr /i "Go" >nul && (
    echo [INFO]  Installing Go...
    scoop install go
)

echo !MISSING! | findstr /i "CMake" >nul && (
    echo [INFO]  Installing CMake...
    scoop install cmake
)

echo !MISSING! | findstr /i "reuse" >nul && (
    echo [INFO]  Installing reuse via pip...
    where pip >nul 2>&1 || scoop install python
    pip install reuse
)

echo !MISSING! | findstr /i "lefthook" >nul && (
    echo [INFO]  Installing lefthook...
    where go >nul 2>&1 && (
        go install github.com/evilmartians/lefthook@latest
        set "PATH=%GOPATH%\bin;%USERPROFILE%\go\bin;%PATH%"
    ) || (
        scoop install lefthook
    )
)

echo.
echo [ OK ]  Package installation complete
echo.

rem ── C/C++ Libraries via Scoop ────────────────────────────────────────────────
:install_libs
echo [INFO]  Installing C/C++ libraries via Scoop...

scoop bucket add extras >nul 2>&1

echo [INFO]  Installing extra-cmake-modules...
scoop install extras/extra-cmake-modules 2>nul || echo [WARN]  extra-cmake-modules not available via scoop, install manually.

echo [INFO]  Installing libarchive...
scoop install main/libarchive 2>nul || echo [WARN]  libarchive not available via scoop, install manually.

echo [INFO]  Installing pkg-config...
scoop install main/pkg-config 2>nul || echo [WARN]  pkg-config not available via scoop, install manually.

echo.
echo [ OK ]  System libraries installed
echo [INFO]  Monorepo libraries (tomlplusplus, cmark, etc.) will be built by build-deps.ps1
echo.

rem ── Lefthook Setup ─────────────────────────────────────────────────────────
:setup_lefthook
where lefthook >nul 2>&1
if errorlevel 1 (
    echo [ERR]  lefthook is not available. Cannot set up git hooks. 1>&2
    exit /b 1
)

echo [INFO]  Setting up lefthook...
lefthook install
if errorlevel 1 (
    echo [ERR]  lefthook install failed. 1>&2
    exit /b 1
)
echo [ OK ]  Lefthook hooks installed into .git\hooks
echo.

echo ---------------------------------------------
echo [ OK ]  Bootstrap successful! You're all set.
echo.

endlocal
exit /b 0

rem ── Helper Functions ────────────────────────────────────────────────────────
:check_cmd
set "NAME=%~1"
set "CMD=%~2"
where %CMD% >nul 2>&1
if errorlevel 1 (
    echo [WARN]  %NAME% is NOT installed
    set "MISSING=!MISSING! %NAME%"
) else (
    for /f "delims=" %%i in ('where %CMD%') do (
        echo [ OK ]  %NAME% is installed ^(%%i^)
        goto :eof
    )
)
goto :eof
