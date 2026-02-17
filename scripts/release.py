#!/usr/bin/env python3
"""
PixelGrab — Cross-platform Release Script

One-command release pipeline:
  1. Bump version in CMakeLists.txt
  2. Build & package (cmake)
  3. Check release notes in dist/
  4. Create GitHub Release via `gh` CLI

Works on Windows, macOS, and Linux. Requires: Python 3.6+, CMake, gh CLI.

Usage:
  python scripts/release.py patch
  python scripts/release.py minor
  python scripts/release.py 2.0.0
  python scripts/release.py patch --skip-build
  python scripts/release.py patch --notes dist/release-notes-1.0.2.md
  python scripts/release.py patch --dry-run
"""

import argparse
import glob
import os
import platform
import re
import shutil
import subprocess
import sys

# =====================================================================
# Constants
# =====================================================================

PROJECT_DIR = os.path.normpath(os.path.join(os.path.dirname(__file__), ".."))
CMAKE_FILE = os.path.join(PROJECT_DIR, "CMakeLists.txt")
BUILD_DIR = os.path.join(PROJECT_DIR, "build")
PACKAGES_DIR = os.path.join(BUILD_DIR, "packages")
DIST_DIR = os.path.join(PROJECT_DIR, "dist")

VERSION_RE = re.compile(
    r"(project\s*\(\s*\S+\s*\n\s*VERSION\s+)(\d+\.\d+\.\d+)",
    re.MULTILINE,
)

REPO_RE = re.compile(r'PIXELGRAB_GITHUB_REPO\s+"([^"]+)"')

# Platform-specific package glob suffixes
PLATFORM_TAG = {
    "Windows": "win64",
    "Darwin": "Darwin",
    "Linux": "Linux",
}.get(platform.system(), platform.system())


def color(text: str, code: str) -> str:
    """ANSI color wrapper (no-op on Windows without VT support)."""
    if sys.stdout.isatty():
        return f"\033[{code}m{text}\033[0m"
    return text


def green(t: str) -> str:
    return color(t, "32")


def cyan(t: str) -> str:
    return color(t, "36")


def yellow(t: str) -> str:
    return color(t, "33")


def red(t: str) -> str:
    return color(t, "31")


def step_header(num: int, total: int, desc: str) -> None:
    print(f"\n{cyan(f'=== [{num}/{total}] {desc} ===')}")


def run(cmd: list, dry_run: bool = False, check: bool = True, **kwargs):
    """Run a subprocess command, with dry-run support."""
    display = " ".join(cmd)
    if dry_run:
        print(f"  {yellow('[DRY RUN]')} {display}")
        return None
    print(f"  > {display}")
    result = subprocess.run(cmd, **kwargs)
    if check and result.returncode != 0:
        print(red(f"  ERROR: Command failed (exit {result.returncode})"))
        sys.exit(result.returncode)
    return result


# =====================================================================
# Step 1: Bump version
# =====================================================================

def read_current_version() -> tuple:
    """Read current version from CMakeLists.txt, returns (major, minor, patch)."""
    with open(CMAKE_FILE, "r", encoding="utf-8") as f:
        content = f.read()
    m = VERSION_RE.search(content)
    if not m:
        print(red("ERROR: Could not find VERSION x.y.z in CMakeLists.txt"))
        sys.exit(1)
    parts = m.group(2).split(".")
    return int(parts[0]), int(parts[1]), int(parts[2])


def compute_new_version(arg: str, cur: tuple) -> str:
    """Compute new version string from bump type or explicit version."""
    major, minor, patch = cur
    if arg == "patch":
        return f"{major}.{minor}.{patch + 1}"
    elif arg == "minor":
        return f"{major}.{minor + 1}.0"
    elif arg == "major":
        return f"{major + 1}.0.0"
    else:
        # Explicit version
        if not re.match(r"^\d+\.\d+\.\d+$", arg):
            print(red(f"ERROR: Invalid version '{arg}'. Expected: major.minor.patch"))
            sys.exit(1)
        return arg


def bump_version(new_version: str, dry_run: bool = False) -> None:
    """Replace version in CMakeLists.txt."""
    with open(CMAKE_FILE, "r", encoding="utf-8") as f:
        content = f.read()

    new_content = VERSION_RE.sub(rf"\g<1>{new_version}", content, count=1)

    if new_content == content:
        print(yellow(f"  Version already set to {new_version}, no change needed."))
        return

    if dry_run:
        print(f"  {yellow('[DRY RUN]')} Would update CMakeLists.txt: VERSION {new_version}")
        return

    with open(CMAKE_FILE, "w", encoding="utf-8") as f:
        f.write(new_content)
    print(f"  Updated CMakeLists.txt → VERSION {new_version}")


# =====================================================================
# Step 2: Build & package
# =====================================================================

def kill_process(name: str) -> None:
    """Kill a running process by name (best-effort, no error on failure)."""
    system = platform.system()
    try:
        if system == "Windows":
            subprocess.run(
                ["taskkill", "/f", "/im", f"{name}.exe"],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
        else:
            subprocess.run(
                ["pkill", "-f", name],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
    except FileNotFoundError:
        pass


def build_and_package(dry_run: bool = False) -> None:
    """Configure, build, install, and package via CMake."""
    build_type = "Release"

    # Kill PixelGrab if running (prevents linker lock on Windows)
    print("  Checking for running PixelGrab process...")
    kill_process("PixelGrab")

    # Configure
    print(f"\n  [1/4] Configuring CMake ({build_type})...")
    run(
        [
            "cmake",
            "-S", PROJECT_DIR,
            "-B", BUILD_DIR,
            f"-DCMAKE_BUILD_TYPE={build_type}",
            "-DPIXELGRAB_BUILD_EXAMPLES=ON",
            "-DPIXELGRAB_BUILD_TESTS=OFF",
        ],
        dry_run=dry_run,
    )

    # Build
    print(f"\n  [2/4] Building...")
    run(
        ["cmake", "--build", BUILD_DIR, "--config", build_type, "--parallel"],
        dry_run=dry_run,
    )

    # Install
    print(f"\n  [3/4] Installing to staging area...")
    run(
        [
            "cmake", "--install", BUILD_DIR,
            "--config", build_type,
            "--prefix", os.path.join(BUILD_DIR, "install"),
        ],
        dry_run=dry_run,
    )

    # Package
    print(f"\n  [4/4] Packaging...")
    system = platform.system()
    if system == "Windows":
        generators = ["NSIS", "ZIP"]
    elif system == "Darwin":
        generators = ["DragNDrop", "ZIP"]
    else:
        generators = ["TGZ", "DEB"]

    for gen in generators:
        run(
            ["cpack", "-C", build_type, "-G", gen],
            dry_run=dry_run,
            check=False,  # packaging failure is non-fatal (e.g. NSIS not installed)
            cwd=BUILD_DIR if not dry_run else None,
        )


# =====================================================================
# Step 3: Locate release notes
# =====================================================================

def find_release_notes(version: str, notes_arg: str | None) -> str:
    """Find or prompt for release notes file path."""
    # User-specified notes file
    if notes_arg:
        path = os.path.abspath(notes_arg)
        if not os.path.isfile(path):
            print(red(f"ERROR: Notes file not found: {path}"))
            sys.exit(1)
        return path

    # Convention: dist/release-notes-{version}.md
    expected = os.path.join(DIST_DIR, f"release-notes-{version}.md")
    if os.path.isfile(expected):
        print(f"  Found: {os.path.relpath(expected, PROJECT_DIR)}")
        return expected

    # Fallback: build/release-notes-{version}.md
    fallback = os.path.join(BUILD_DIR, f"release-notes-{version}.md")
    if os.path.isfile(fallback):
        print(f"  Found: {os.path.relpath(fallback, PROJECT_DIR)}")
        return fallback

    print(yellow(f"  Release notes not found at:"))
    print(yellow(f"    {os.path.relpath(expected, PROJECT_DIR)}"))
    print(yellow(f"    {os.path.relpath(fallback, PROJECT_DIR)}"))
    print()

    # Create a template for the user
    os.makedirs(DIST_DIR, exist_ok=True)
    with open(expected, "w", encoding="utf-8") as f:
        f.write(f"## PixelGrab {version}\n\n### Changes\n\n- \n")
    print(f"  Created template: {os.path.relpath(expected, PROJECT_DIR)}")
    print(yellow("  Please edit the release notes, then re-run this script."))
    sys.exit(1)


# =====================================================================
# Step 4: GitHub Release
# =====================================================================

def read_github_repo() -> str:
    """Read PIXELGRAB_GITHUB_REPO from CMakeLists.txt."""
    with open(CMAKE_FILE, "r", encoding="utf-8") as f:
        content = f.read()
    m = REPO_RE.search(content)
    return m.group(1) if m else "Yangqihu0328/loong-pixelgrab"


def find_packages(version: str) -> list:
    """Find package files for the given version."""
    patterns = [
        os.path.join(PACKAGES_DIR, f"PixelGrab-{version}-*.exe"),
        os.path.join(PACKAGES_DIR, f"PixelGrab-{version}-*.zip"),
        os.path.join(PACKAGES_DIR, f"PixelGrab-{version}-*.dmg"),
        os.path.join(PACKAGES_DIR, f"PixelGrab-{version}-*.tar.gz"),
        os.path.join(PACKAGES_DIR, f"PixelGrab-{version}-*.deb"),
    ]
    files = []
    for pat in patterns:
        files.extend(glob.glob(pat))

    if not files:
        # Broader match
        broader = glob.glob(os.path.join(PACKAGES_DIR, "PixelGrab-*"))
        if broader:
            print(yellow(f"  No packages for v{version}, but found:"))
            for f in broader:
                print(f"    {os.path.basename(f)}")
            ans = input("  Use these instead? (y/N): ").strip().lower()
            if ans == "y":
                return broader
        print(red(f"  ERROR: No package files found in {PACKAGES_DIR}"))
        sys.exit(1)

    return files


def create_github_release(
    version: str, notes_path: str, packages: list,
    repo: str, dry_run: bool = False,
) -> None:
    """Create a GitHub Release using the gh CLI."""
    tag = f"v{version}"

    # Check if release already exists
    result = subprocess.run(
        ["gh", "release", "view", tag, "--repo", repo],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    if result.returncode == 0:
        print(yellow(f"  Release {tag} already exists. Deleting..."))
        run(["gh", "release", "delete", tag, "--repo", repo, "--yes"],
            dry_run=dry_run, check=False)

    cmd = [
        "gh", "release", "create", tag,
        "--repo", repo,
        "--title", f"PixelGrab {version}",
        "--notes-file", notes_path,
    ]

    # If no local git repo, target main branch
    if not os.path.isdir(os.path.join(PROJECT_DIR, ".git")):
        cmd.extend(["--target", "main"])

    # Append package files
    cmd.extend(packages)

    run(cmd, dry_run=dry_run)


# =====================================================================
# Pre-flight checks
# =====================================================================

def preflight() -> None:
    """Verify required tools are available."""
    # cmake
    if not shutil.which("cmake"):
        print(red("ERROR: cmake is not installed or not in PATH."))
        print("  Install from: https://cmake.org/download/")
        sys.exit(1)

    # gh
    if not shutil.which("gh"):
        print(red("ERROR: GitHub CLI (gh) is not installed."))
        print("  Install from: https://cli.github.com/")
        sys.exit(1)

    # gh auth
    result = subprocess.run(
        ["gh", "auth", "status"],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    if result.returncode != 0:
        print(red("ERROR: GitHub CLI is not authenticated."))
        print("  Run: gh auth login")
        sys.exit(1)


# =====================================================================
# Main
# =====================================================================

def main() -> None:
    parser = argparse.ArgumentParser(
        description="PixelGrab cross-platform release script.",
        epilog="Examples:\n"
               "  python scripts/release.py patch\n"
               "  python scripts/release.py minor --skip-build\n"
               "  python scripts/release.py 2.0.0 --dry-run\n"
               "  python scripts/release.py patch --notes dist/release-notes-1.0.2.md\n",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "version",
        help="Version bump type (patch/minor/major) or explicit version (e.g. 2.0.0)",
    )
    parser.add_argument(
        "--skip-build", action="store_true",
        help="Skip build & package step (use existing packages)",
    )
    parser.add_argument(
        "--notes",
        help="Path to release notes markdown file "
             "(default: dist/release-notes-{version}.md)",
    )
    parser.add_argument(
        "--dry-run", action="store_true",
        help="Preview mode: show what would be done without executing",
    )

    args = parser.parse_args()
    dry_run = args.dry_run

    # ---- Banner ----
    repo = read_github_repo()
    print()
    print(green("============================================"))
    print(green("  PixelGrab Release Script"))
    print(green("============================================"))
    print(f"  Project:    {PROJECT_DIR}")
    print(f"  Platform:   {platform.system()} {platform.machine()}")
    print(f"  GitHub:     {repo}")
    if dry_run:
        print(f"  Mode:       {yellow('DRY RUN')}")

    # ---- Pre-flight ----
    if not dry_run:
        preflight()

    total = 3 if args.skip_build else 4
    step = 0

    # ---- Step 1: Bump version ----
    step += 1
    step_header(step, total, "Bump Version")

    cur = read_current_version()
    cur_str = f"{cur[0]}.{cur[1]}.{cur[2]}"
    new_version = compute_new_version(args.version, cur)

    print(f"  Current: {cur_str}")
    print(f"  New:     {new_version}")
    bump_version(new_version, dry_run=dry_run)

    # ---- Step 2: Build & package ----
    if not args.skip_build:
        step += 1
        step_header(step, total, "Build & Package")
        build_and_package(dry_run=dry_run)
    else:
        print(f"\n  {yellow('Build skipped (--skip-build)')}")

    # ---- Step 3: Release notes ----
    step += 1
    step_header(step, total, "Release Notes")
    notes_path = find_release_notes(new_version, args.notes)
    print(f"  Using: {os.path.relpath(notes_path, PROJECT_DIR)}")

    # ---- Step 4: GitHub Release ----
    step += 1
    step_header(step, total, "Create GitHub Release")

    packages = find_packages(new_version) if not dry_run else []
    if not dry_run:
        print(f"  Packages ({len(packages)}):")
        for p in packages:
            print(f"    {os.path.basename(p)}")
    else:
        print(f"  {yellow('[DRY RUN]')} Would upload packages from {PACKAGES_DIR}")

    create_github_release(new_version, notes_path, packages, repo, dry_run=dry_run)

    # ---- Done ----
    tag = f"v{new_version}"
    print()
    print(green("============================================"))
    print(green(f"  Release {new_version} complete!"))
    print(green("============================================"))
    print(f"  Version:    {new_version}")
    print(f"  Tag:        {tag}")
    print(f"  Repository: {repo}")
    print(f"  Release:    https://github.com/{repo}/releases/tag/{tag}")
    print()


if __name__ == "__main__":
    main()
