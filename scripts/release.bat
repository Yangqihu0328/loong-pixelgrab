@echo off
REM ===================================================================
REM  PixelGrab — One-click Release Script (Windows batch)
REM
REM  Full release pipeline:
REM    1. Bump version via bump_version.bat
REM    2. Kill running PixelGrab process (if any)
REM    3. Build & package via build_and_package.bat
REM    4. (If git repo) Git commit + tag + push
REM    5. Prepare release notes
REM    6. Create GitHub Release via `gh` CLI
REM
REM  Works both WITH and WITHOUT a local git repository.
REM
REM  Usage:
REM    scripts\release.bat patch                          # bump patch
REM    scripts\release.bat minor                          # bump minor
REM    scripts\release.bat major                          # bump major
REM    scripts\release.bat 2.0.0                          # explicit version
REM    scripts\release.bat patch --skip-build             # reuse existing packages
REM    scripts\release.bat patch --notes build\notes.md   # custom release notes
REM    scripts\release.bat patch --dry-run                # preview only
REM ===================================================================

setlocal enabledelayedexpansion

REM Resolve project root
set "SCRIPT_DIR=%~dp0"
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
for %%I in ("%SCRIPT_DIR%\..") do set "PROJECT_DIR=%%~fI"

set "BUILD_DIR=%PROJECT_DIR%\build"
set "PACKAGES_DIR=%BUILD_DIR%\packages"

REM ---- Parse arguments ----
set "VERSION_ARG="
set "SKIP_BUILD=0"
set "NOTES_FILE="
set "DRY_RUN=0"

:parse_args
if "%~1"=="" goto done_args
if /i "%~1"=="--skip-build" (
    set "SKIP_BUILD=1"
    shift
    goto parse_args
)
if /i "%~1"=="--notes" (
    set "NOTES_FILE=%~2"
    shift
    shift
    goto parse_args
)
if /i "%~1"=="--dry-run" (
    set "DRY_RUN=1"
    shift
    goto parse_args
)
if not defined VERSION_ARG (
    set "VERSION_ARG=%~1"
    shift
    goto parse_args
)
shift
goto parse_args

:done_args

if "%VERSION_ARG%"=="" (
    echo ERROR: Please specify: patch, minor, major, or an explicit version.
    echo.
    echo Usage:
    echo   scripts\release.bat patch
    echo   scripts\release.bat minor
    echo   scripts\release.bat patch --skip-build
    echo   scripts\release.bat patch --notes build\release-notes.md
    echo   scripts\release.bat patch --dry-run
    exit /b 1
)

REM ---- Detect GitHub repo from CMakeLists.txt ----
set "GH_REPO=Yangqihu0328/loong-pixelgrab"
for /f "tokens=*" %%R in ('findstr "PIXELGRAB_GITHUB_REPO" "%PROJECT_DIR%\CMakeLists.txt" 2^>nul') do (
    echo %%R | findstr /r /c:"\"[^\"]*\"" >nul 2>&1
    for /f "tokens=2 delims=^"" %%Q in ("%%R") do set "GH_REPO=%%Q"
)

REM ---- Detect local git repo ----
set "HAS_GIT=0"
if exist "%PROJECT_DIR%\.git" set "HAS_GIT=1"

REM ---- Banner ----
echo.
echo ============================================
echo   PixelGrab Release Script
echo ============================================
echo.
echo   Project:    %PROJECT_DIR%
echo   GitHub:     %GH_REPO%
echo   Local git:  %HAS_GIT%
if "%DRY_RUN%"=="1" echo   Mode:       DRY RUN

REM ---- Pre-flight: check gh CLI ----
where gh >nul 2>&1
if errorlevel 1 (
    echo ERROR: GitHub CLI ^(gh^) is not installed. Install from https://cli.github.com/
    exit /b 1
)
gh auth status >nul 2>&1
if errorlevel 1 (
    echo ERROR: GitHub CLI is not authenticated. Run: gh auth login
    exit /b 1
)

REM ---- Pre-flight: check for uncommitted changes ----
if "%HAS_GIT%"=="1" if "%DRY_RUN%"=="0" (
    set "HAS_CHANGES=0"
    for /f %%S in ('git -C "%PROJECT_DIR%" status --porcelain 2^>nul') do set "HAS_CHANGES=1"
    if "!HAS_CHANGES!"=="1" (
        echo WARNING: Working directory has uncommitted changes.
        git -C "%PROJECT_DIR%" status --short
        set /p "CONFIRM=Continue anyway? (y/N): "
        if /i not "!CONFIRM!"=="y" (
            echo Aborted.
            exit /b 0
        )
    )
)

REM Compute total steps
set "TOTAL_STEPS=4"
if "%HAS_GIT%"=="1" set "TOTAL_STEPS=6"
set "STEP=0"

REM =====================================================================
REM Step: Bump version
REM =====================================================================
set /a STEP+=1
echo.
echo === [%STEP%/%TOTAL_STEPS%] Bump Version ===

if "%DRY_RUN%"=="1" (
    echo   [DRY RUN] call "%SCRIPT_DIR%\bump_version.bat" %VERSION_ARG%
) else (
    call "%SCRIPT_DIR%\bump_version.bat" %VERSION_ARG%
    if errorlevel 1 (
        echo ERROR: Version bump failed.
        exit /b 1
    )
)

REM Read the new version from CMakeLists.txt
set "NEW_VERSION="
for /f "tokens=1,2" %%A in ('findstr /r /c:"VERSION [0-9]*\.[0-9]*\.[0-9]*" "%PROJECT_DIR%\CMakeLists.txt"') do (
    if "%%A"=="VERSION" set "NEW_VERSION=%%B"
)
if not defined NEW_VERSION (
    echo ERROR: Could not read version from CMakeLists.txt after bump.
    exit /b 1
)

set "TAG_NAME=v%NEW_VERSION%"
echo.
echo   Version:  %NEW_VERSION%
echo   Tag:      %TAG_NAME%

REM =====================================================================
REM Step: Build & Package
REM =====================================================================
set /a STEP+=1
echo.
echo === [!STEP!/%TOTAL_STEPS%] Build ^& Package ===

if "%SKIP_BUILD%"=="1" (
    echo   Skipped ^(--skip-build^)
) else (
    REM Kill PixelGrab if running (prevents LNK1104)
    tasklist /fi "imagename eq PixelGrab.exe" 2>nul | findstr /i "PixelGrab.exe" >nul 2>&1
    if not errorlevel 1 (
        echo   Stopping running PixelGrab process...
        if "%DRY_RUN%"=="0" (
            taskkill /f /im PixelGrab.exe >nul 2>&1
            timeout /t 1 /nobreak >nul
        )
    )

    if "%DRY_RUN%"=="1" (
        echo   [DRY RUN] call "%SCRIPT_DIR%\build_and_package.bat" Release
    ) else (
        call "%SCRIPT_DIR%\build_and_package.bat" Release
        if errorlevel 1 (
            echo ERROR: Build failed.
            exit /b 1
        )
    )
)

REM ---- Collect package files ----
set "ASSET_COUNT=0"
set "ASSET_LIST="

if exist "%PACKAGES_DIR%" (
    for %%F in ("%PACKAGES_DIR%\PixelGrab-%NEW_VERSION%-*.exe") do (
        if exist "%%F" (
            set /a ASSET_COUNT+=1
            set "ASSET_LIST=!ASSET_LIST! "%%F""
        )
    )
    for %%F in ("%PACKAGES_DIR%\PixelGrab-%NEW_VERSION%-*.zip") do (
        if exist "%%F" (
            set /a ASSET_COUNT+=1
            set "ASSET_LIST=!ASSET_LIST! "%%F""
        )
    )
)

if %ASSET_COUNT% equ 0 if "%DRY_RUN%"=="0" (
    echo WARNING: No package files found for version %NEW_VERSION% in %PACKAGES_DIR%
    REM Try broader match
    if exist "%PACKAGES_DIR%" (
        for %%F in ("%PACKAGES_DIR%\PixelGrab-*.exe") do (
            if exist "%%F" (
                set /a ASSET_COUNT+=1
                set "ASSET_LIST=!ASSET_LIST! "%%F""
                echo   Found: %%~nxF
            )
        )
        for %%F in ("%PACKAGES_DIR%\PixelGrab-*.zip") do (
            if exist "%%F" (
                set /a ASSET_COUNT+=1
                set "ASSET_LIST=!ASSET_LIST! "%%F""
                echo   Found: %%~nxF
            )
        )
    )
)

echo.
echo   Packages to upload: %ASSET_COUNT% file^(s^)
for %%F in (%ASSET_LIST%) do echo     %%~nxF

REM =====================================================================
REM Step: Git commit + tag + push (only if git repo)
REM =====================================================================
if "%HAS_GIT%"=="1" (
    set /a STEP+=1
    echo.
    echo === [!STEP!/%TOTAL_STEPS%] Git Commit ^& Tag ===

    if "%DRY_RUN%"=="1" (
        echo   [DRY RUN] git add CMakeLists.txt
        echo   [DRY RUN] git commit -m "chore: bump version to %NEW_VERSION%"
        echo   [DRY RUN] git tag -a %TAG_NAME% -m "Release %NEW_VERSION%"
    ) else (
        git -C "%PROJECT_DIR%" add CMakeLists.txt
        git -C "%PROJECT_DIR%" commit -m "chore: bump version to %NEW_VERSION%"
        if errorlevel 1 (
            echo WARNING: Git commit may have failed. Continuing...
        )
        git -C "%PROJECT_DIR%" tag -a %TAG_NAME% -m "Release %NEW_VERSION%"
    )

    set /a STEP+=1
    echo.
    echo === [!STEP!/%TOTAL_STEPS%] Git Push ===

    if "%DRY_RUN%"=="1" (
        echo   [DRY RUN] git push
        echo   [DRY RUN] git push --tags --force
    ) else (
        git -C "%PROJECT_DIR%" push
        git -C "%PROJECT_DIR%" push --tags --force
    )
)

REM =====================================================================
REM Step: Prepare release notes
REM =====================================================================
set /a STEP+=1
echo.
echo === [!STEP!/%TOTAL_STEPS%] Prepare Release Notes ===

set "NOTES_PATH="

if defined NOTES_FILE (
    if exist "%NOTES_FILE%" (
        set "NOTES_PATH=%NOTES_FILE%"
        echo   Using custom notes: !NOTES_PATH!
    ) else (
        echo WARNING: Notes file not found: %NOTES_FILE% - generating default.
    )
)

if not defined NOTES_PATH (
    set "NOTES_PATH=%BUILD_DIR%\release-notes-%NEW_VERSION%.md"

    REM Generate release notes
    > "!NOTES_PATH!" echo ## PixelGrab %NEW_VERSION%
    >> "!NOTES_PATH!" echo.

    set "HAS_COMMITS=0"

    if "%HAS_GIT%"=="1" (
        REM Find previous tag
        set "PREV_TAG="
        for /f "tokens=*" %%T in ('git -C "%PROJECT_DIR%" tag --sort^=-version:refname 2^>nul') do (
            if not defined PREV_TAG if not "%%T"=="%TAG_NAME%" set "PREV_TAG=%%T"
        )

        REM Get commit range
        set "GIT_RANGE=%TAG_NAME%"
        if defined PREV_TAG set "GIT_RANGE=!PREV_TAG!..%TAG_NAME%"

        REM Collect commits
        for /f "tokens=1,* delims= " %%A in ('git -C "%PROJECT_DIR%" log --oneline --no-merges !GIT_RANGE! 2^>nul') do (
            set "HAS_COMMITS=1"
            >> "!NOTES_PATH!" echo - %%B
        )

        REM If no commits in range, try recent commits
        if "!HAS_COMMITS!"=="0" (
            for /f "tokens=1,* delims= " %%A in ('git -C "%PROJECT_DIR%" log --oneline --no-merges -20 2^>nul') do (
                set "HAS_COMMITS=1"
                >> "!NOTES_PATH!" echo - %%B
            )
        )
    )

    if "!HAS_COMMITS!"=="0" (
        >> "!NOTES_PATH!" echo Release %NEW_VERSION%
    )

    echo   Generated: !NOTES_PATH!
    echo.
    type "!NOTES_PATH!"
)

REM =====================================================================
REM Step: Create GitHub Release
REM =====================================================================
set /a STEP+=1
echo.
echo === [!STEP!/%TOTAL_STEPS%] Create GitHub Release ===

REM Check if release already exists
gh release view %TAG_NAME% --repo %GH_REPO% >nul 2>&1
if not errorlevel 1 (
    echo WARNING: Release %TAG_NAME% already exists. Deleting and re-creating...
    if "%DRY_RUN%"=="0" (
        gh release delete %TAG_NAME% --repo %GH_REPO% --yes >nul 2>&1
    )
)

REM Build gh command arguments
set "GH_ARGS=release create %TAG_NAME% --repo %GH_REPO% --title "PixelGrab %NEW_VERSION%" --notes-file "!NOTES_PATH!""

REM If no local git, specify target branch
if "%HAS_GIT%"=="0" (
    set "GH_ARGS=!GH_ARGS! --target main"
)

REM Append asset files
for %%F in (%ASSET_LIST%) do (
    set "GH_ARGS=!GH_ARGS! %%F"
)

echo   Command: gh !GH_ARGS!

if "%DRY_RUN%"=="1" (
    echo   [DRY RUN] Would execute the above command.
) else (
    gh !GH_ARGS!
    if errorlevel 1 (
        echo ERROR: GitHub release creation failed.
        exit /b 1
    )
)

REM =====================================================================
REM Done
REM =====================================================================
echo.
echo ============================================
echo   Release %NEW_VERSION% complete!
echo ============================================
echo.
echo   Version:    %NEW_VERSION%
echo   Tag:        %TAG_NAME%
echo   Repository: %GH_REPO%
echo   Packages:   %ASSET_COUNT% file^(s^) uploaded
echo.
echo   View release: https://github.com/%GH_REPO%/releases/tag/%TAG_NAME%
echo.

endlocal
