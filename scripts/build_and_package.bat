@echo off
REM ===================================================================
REM  PixelGrab — Build & Package Script (Windows)
REM  Usage:
REM    scripts\build_and_package.bat           — Release build + NSIS installer + ZIP
REM    scripts\build_and_package.bat Debug     — Debug build (no packaging)
REM    scripts\build_and_package.bat Release   — Release build + packaging
REM    scripts\build_and_package.bat clean     — Clean build directory
REM ===================================================================

setlocal enabledelayedexpansion

REM Resolve project root: parent of the directory containing this script.
set "SCRIPT_DIR=%~dp0"
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
for %%I in ("%SCRIPT_DIR%\..") do set "PROJECT_DIR=%%~fI"

set "BUILD_DIR=%PROJECT_DIR%\build"
set "BUILD_TYPE=%~1"

REM Default to Release
if "%BUILD_TYPE%"=="" set "BUILD_TYPE=Release"

REM ---- Clean ----
if /i "%BUILD_TYPE%"=="clean" (
    echo [PixelGrab] Cleaning build directory...
    if exist "%BUILD_DIR%" rd /s /q "%BUILD_DIR%"
    echo [PixelGrab] Clean complete.
    goto :eof
)

echo.
echo ===================================================================
echo  PixelGrab Build ^& Package Script
echo  Project:    %PROJECT_DIR%
echo  Build Dir:  %BUILD_DIR%
echo  Build Type: %BUILD_TYPE%
echo ===================================================================
echo.

REM ---- Pre-flight: Kill running PixelGrab (prevents LNK1104) ----
tasklist /fi "imagename eq PixelGrab.exe" 2>nul | findstr /i "PixelGrab.exe" >nul 2>&1
if not errorlevel 1 (
    echo [PixelGrab] Stopping running PixelGrab process...
    taskkill /f /im PixelGrab.exe >nul 2>&1
    timeout /t 1 /nobreak >nul
)

REM ---- Step 1: Configure ----
echo [1/4] Configuring CMake (%BUILD_TYPE%)...
cmake -S "%PROJECT_DIR%" -B "%BUILD_DIR%" ^
    -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
    -DPIXELGRAB_BUILD_EXAMPLES=ON ^
    -DPIXELGRAB_BUILD_TESTS=OFF

if %ERRORLEVEL% neq 0 (
    echo [ERROR] CMake configuration failed.
    exit /b 1
)

REM ---- Step 2: Build ----
echo.
echo [2/4] Building...
cmake --build "%BUILD_DIR%" --config %BUILD_TYPE% --parallel

if %ERRORLEVEL% neq 0 (
    echo [ERROR] Build failed.
    exit /b 1
)

REM ---- Step 3: Install (staging) ----
echo.
echo [3/4] Installing to staging area...
cmake --install "%BUILD_DIR%" --config %BUILD_TYPE% --prefix "%BUILD_DIR%\install"

if %ERRORLEVEL% neq 0 (
    echo [ERROR] Install failed.
    exit /b 1
)

REM ---- Step 4: Package ----
if /i not "%BUILD_TYPE%"=="Release" goto skip_package

echo.
echo [4/4] Packaging...
cd /d "%BUILD_DIR%"
cpack -C %BUILD_TYPE% -G NSIS
if %ERRORLEVEL% neq 0 (
    echo [WARNING] NSIS packaging failed. NSIS might not be installed.
    echo           Install NSIS from https://nsis.sourceforge.io/
)
cpack -C %BUILD_TYPE% -G ZIP
if %ERRORLEVEL% neq 0 (
    echo [WARNING] ZIP packaging failed.
)

echo.
echo ===================================================================
echo  Packages created in: %BUILD_DIR%\packages\
echo ===================================================================
dir /b "%BUILD_DIR%\packages\PixelGrab-*" 2>nul
goto done_package

:skip_package
echo.
echo [4/4] Skipping packaging (only for Release builds).

:done_package

echo.
echo [PixelGrab] Build complete!
echo   Executable: %BUILD_DIR%\bin\%BUILD_TYPE%\PixelGrab.exe
echo.

endlocal
