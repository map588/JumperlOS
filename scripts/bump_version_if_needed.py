#!/usr/bin/env python3
"""Bump the VERSION file when the current version was already released.

The release workflow tags every release with the exact contents of the
VERSION file (e.g. `5.7.0.1`). So "did the human bump the version?" reduces
to "does a git tag already exist for the version in VERSION?":

  * No tag yet  -> this is a fresh version, release it as-is.
  * Tag exists  -> the version wasn't bumped, so increment the last numeric
                   component until we reach a version that has no tag yet.

The (possibly updated) value is written back to VERSION so the build bakes the
correct string in and the repo keeps moving forward. When run inside GitHub
Actions the resulting `version` and `bumped` values are emitted to
$GITHUB_OUTPUT for later steps.

Usage:
  python scripts/bump_version_if_needed.py            # consult `git tag`
  python scripts/bump_version_if_needed.py --dry-run  # don't write VERSION
"""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
VERSION_FILE = ROOT / "VERSION"


def existing_tags() -> set[str]:
    """Return the set of tag names known to the local git repo."""
    try:
        out = subprocess.check_output(
            ["git", "tag", "--list"], cwd=ROOT, text=True
        )
    except (subprocess.CalledProcessError, FileNotFoundError):
        return set()
    return {line.strip() for line in out.splitlines() if line.strip()}


def increment(version: str) -> str:
    """Increment the last purely-numeric, dot-separated component."""
    parts = version.split(".")
    for i in range(len(parts) - 1, -1, -1):
        if parts[i].isdigit():
            parts[i] = str(int(parts[i]) + 1)
            return ".".join(parts)
    raise SystemExit(f"VERSION '{version}' has no numeric component to bump")


def emit_output(version: str, bumped: bool) -> None:
    gh_out = os.environ.get("GITHUB_OUTPUT")
    if not gh_out:
        return
    with open(gh_out, "a", encoding="utf-8") as fh:
        fh.write(f"version={version}\n")
        fh.write(f"bumped={'true' if bumped else 'false'}\n")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="compute the next version but do not write the VERSION file",
    )
    args = parser.parse_args()

    current = VERSION_FILE.read_text(encoding="utf-8").strip()
    if not current:
        raise SystemExit("VERSION file is empty")

    tags = existing_tags()
    final = current
    bumped = False
    while final in tags:
        final = increment(final)
        bumped = True

    if bumped and not args.dry_run:
        # Match the existing file style: no trailing newline.
        VERSION_FILE.write_text(final, encoding="utf-8")

    if bumped:
        print(f"VERSION '{current}' already released; bumped to '{final}'")
    else:
        print(f"VERSION '{current}' is new; releasing as-is")

    emit_output(final, bumped)
    return 0


if __name__ == "__main__":
    sys.exit(main())
