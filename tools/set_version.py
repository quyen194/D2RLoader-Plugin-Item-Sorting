#!/usr/bin/env python3
"""Show or change the plugin version.

The version lives in one place: `project(ItemSorting VERSION x.y.z ...)` in
CMakeLists.txt. CMake turns it into the generated plugin_version.h, which feeds
both the DLL's VERSIONINFO resource (Explorer > Properties > Details: File
version / Product version) and the SDK PluginInfo.version string.

Usage (from anywhere):
    python tools/set_version.py              # print the current version
    python tools/set_version.py 1.2.3        # set an explicit version
    python tools/set_version.py patch        # 1.2.3 -> 1.2.4
    python tools/set_version.py minor        # 1.2.3 -> 1.3.0
    python tools/set_version.py major        # 1.2.3 -> 2.0.0
    python tools/set_version.py patch --commit --tag
        # also `git commit` CMakeLists.txt and create tag vX.Y.Z
        # (push the tag yourself to trigger the Release workflow)

The next build re-runs CMake configure automatically (CMakeLists.txt changed),
so no manual reconfigure is needed.
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
CMAKE_FILE = REPO_ROOT / "CMakeLists.txt"
PROJECT_RE = re.compile(
    r"^(project\(\s*ItemSorting\s+VERSION\s+)(\d+)\.(\d+)\.(\d+)(\b)", re.MULTILINE
)
SEMVER_RE = re.compile(r"^v?(\d+)\.(\d+)\.(\d+)$")


def read_version(text: str) -> tuple[int, int, int]:
    matches = list(PROJECT_RE.finditer(text))
    if len(matches) != 1:
        sys.exit(
            f"error: expected exactly one 'project(ItemSorting VERSION x.y.z ...)' "
            f"line in {CMAKE_FILE.name}, found {len(matches)}"
        )
    m = matches[0]
    return int(m.group(2)), int(m.group(3)), int(m.group(4))


def next_version(current: tuple[int, int, int], spec: str) -> tuple[int, int, int]:
    major, minor, patch = current
    if spec == "major":
        return major + 1, 0, 0
    if spec == "minor":
        return major, minor + 1, 0
    if spec == "patch":
        return major, minor, patch + 1
    m = SEMVER_RE.match(spec)
    if not m:
        sys.exit(f"error: '{spec}' is not major|minor|patch or a MAJOR.MINOR.PATCH version")
    return int(m.group(1)), int(m.group(2)), int(m.group(3))


def git(*args: str) -> None:
    subprocess.run(["git", *args], cwd=REPO_ROOT, check=True)


def main() -> int:
    parser = argparse.ArgumentParser(description="Show or change the plugin version.")
    parser.add_argument("version", nargs="?", help="MAJOR.MINOR.PATCH, or major|minor|patch")
    parser.add_argument("--commit", action="store_true", help="git commit CMakeLists.txt")
    parser.add_argument("--tag", action="store_true", help="create git tag vX.Y.Z (implies --commit)")
    args = parser.parse_args()

    raw = CMAKE_FILE.read_bytes().decode("utf-8")
    current = read_version(raw)
    current_str = ".".join(map(str, current))

    if args.version is None:
        print(current_str)
        return 0

    new = next_version(current, args.version)
    new_str = ".".join(map(str, new))
    if new == current:
        print(f"version is already {new_str}; nothing to do")
        return 0

    updated = PROJECT_RE.sub(lambda m: f"{m.group(1)}{new_str}{m.group(5)}", raw, count=1)
    CMAKE_FILE.write_bytes(updated.encode("utf-8"))
    print(f"{current_str} -> {new_str}  ({CMAKE_FILE.name})")

    if args.commit or args.tag:
        git("add", CMAKE_FILE.name)
        git("commit", "-m", f"Bump version to {new_str}")
    if args.tag:
        git("tag", f"v{new_str}")
        print(f"tagged v{new_str}; publish with: git push origin main v{new_str}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
