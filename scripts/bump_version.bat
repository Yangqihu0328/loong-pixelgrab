@echo off
REM ===================================================================
REM  PixelGrab — Bump Version Script (Windows batch)
REM
REM  Bump the project version number in CMakeLists.txt.
REM  Since version.h is auto-generated via configure_file, only
REM  CMakeLists.txt needs to be edited.
REM
REM  NOTE: Uses "powershell -Command" for file replacement.
REM  This is NOT affected by execution policy (which only blocks .ps1
REM  files). The batch wrapper handles argument parsing and version
REM  arithmetic.
REM
REM  Usage:
REM    scripts\bump_version.bat patch         # 1.0.0 -> 1.0.1
REM    scripts\bump_version.bat minor         # 1.0.0 -> 1.1.0
REM    scripts\bump_version.bat major         # 1.0.0 -> 2.0.0
REM    scripts\bump_version.bat 2.1.0         # explicit version
REM ===================================================================

setlocal enabledelayedexpansion

REM Resolve project root
set "SCRIPT_DIR=%~dp0"
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
for %%I in ("%SCRIPT_DIR%\..") do set "PROJECT_DIR=%%~fI"

set "CMAKE_FILE=%PROJECT_DIR%\CMakeLists.txt"

REM Check argument
set "ARG=%~1"
if "%ARG%"=="" (
    echo ERROR: Please specify: patch, minor, major, or an explicit version ^(e.g. 2.1.0^)
    echo.
    echo Usage:
    echo   scripts\bump_version.bat patch
    echo   scripts\bump_version.bat minor
    echo   scripts\bump_version.bat major
    echo   scripts\bump_version.bat 2.1.0
    exit /b 1
)

REM Check CMakeLists.txt exists
if not exist "%CMAKE_FILE%" (
    echo ERROR: CMakeLists.txt not found at %CMAKE_FILE%
    exit /b 1
)

REM ---- Parse current version from "  VERSION x.y.z" line ----
set "CUR_MAJOR="
set "CUR_MINOR="
set "CUR_PATCH="

for /f "tokens=1,2" %%A in ('findstr /r /c:"VERSION [0-9]*\.[0-9]*\.[0-9]*" "%CMAKE_FILE%"') do (
    if "%%A"=="VERSION" (
        for /f "tokens=1-3 delims=." %%X in ("%%B") do (
            set "CUR_MAJOR=%%X"
            set "CUR_MINOR=%%Y"
            set "CUR_PATCH=%%Z"
        )
    )
)

if not defined CUR_MAJOR (
    echo ERROR: Could not find "VERSION x.y.z" in %CMAKE_FILE%
    exit /b 1
)

set "CUR_VERSION=%CUR_MAJOR%.%CUR_MINOR%.%CUR_PATCH%"
echo Current version: %CUR_VERSION%

REM ---- Determine new version ----
set "NEW_VERSION="

if /i "%ARG%"=="patch" (
    set /a "NEW_PATCH=%CUR_PATCH% + 1"
    set "NEW_VERSION=%CUR_MAJOR%.%CUR_MINOR%.!NEW_PATCH!"
) else if /i "%ARG%"=="minor" (
    set /a "NEW_MINOR=%CUR_MINOR% + 1"
    set "NEW_VERSION=%CUR_MAJOR%.!NEW_MINOR!.0"
) else if /i "%ARG%"=="major" (
    set /a "NEW_MAJOR=%CUR_MAJOR% + 1"
    set "NEW_VERSION=!NEW_MAJOR!.0.0"
) else (
    REM Validate explicit version format (digits.digits.digits)
    echo %ARG%| findstr /r "^[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*$" >nul 2>&1
    if errorlevel 1 (
        echo ERROR: Invalid version format '%ARG%'. Expected: major.minor.patch ^(e.g. 2.1.0^)
        exit /b 1
    )
    set "NEW_VERSION=%ARG%"
)

if "%NEW_VERSION%"=="%CUR_VERSION%" (
    echo Version is already %CUR_VERSION%. Nothing to do.
    exit /b 0
)

echo New version:     %NEW_VERSION%

REM ---- Replace version in CMakeLists.txt ----
REM Using "powershell -Command" for reliable text replacement.
REM This preserves empty lines, encoding, and BOM — unlike batch for/f.
REM NOTE: -Command is NOT blocked by ExecutionPolicy (only .ps1 files are).
powershell -NoProfile -Command ^
    "$f = '%CMAKE_FILE%'; " ^
    "$c = [IO.File]::ReadAllText($f); " ^
    "$c = $c -replace 'VERSION %CUR_VERSION%', 'VERSION %NEW_VERSION%'; " ^
    "[IO.File]::WriteAllText($f, $c)"

if errorlevel 1 (
    echo ERROR: Failed to replace version in %CMAKE_FILE%
    exit /b 1
)

echo.
echo Updated %CMAKE_FILE%
echo Version bumped: %CUR_VERSION% -^> %NEW_VERSION%
echo.
echo Next steps:
echo   1. Rebuild to propagate version to all generated files
echo   2. git add CMakeLists.txt ^&^& git commit -m "chore: bump version to %NEW_VERSION%"
echo   3. git tag v%NEW_VERSION%

endlocal
