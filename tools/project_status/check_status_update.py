#!/usr/bin/env python3
"""Check that subsystem changes are accompanied by project-control updates."""

from __future__ import annotations

import argparse
import posixpath
from pathlib import Path

REQUIRED_CONTROL_FILE = "docs/status/EFIS_NEXT_MASTER_MATRIX.md"

WATCHED_PREFIXES = (
    "include/efis/",
    "src/",
    "qml/",
    "server/",
    "deploy/",
    "config/",
    "tools/",
    "docs/hardware/",
    "docs/protocols/",
    ".github/workflows/",
)

# No self-exemption: dashboard, backup and activation tools are executable code.
# Generated docs/status outputs alone are not watched, so regeneration does not
# recursively require a matrix edit. An accompanying code change still does.


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("files", nargs="*", help="changed repository paths")
    parser.add_argument("--file-list", help="newline separated changed paths")
    args = parser.parse_args()

    changed = list(args.files)
    if args.file_list:
        changed.extend(Path(args.file_list).read_text(encoding="utf-8").splitlines())
    changed = [posixpath.normpath(p.strip().replace("\\", "/"))
               for p in changed if p.strip()]

    substantive = [
        p for p in changed
        if p.startswith(WATCHED_PREFIXES)
    ]
    matrix_updated = REQUIRED_CONTROL_FILE in changed

    if substantive and not matrix_updated:
        print("EFIS project-control check FAILED")
        print("Subsystem files changed without updating project status:")
        for path in substantive:
            print(f"  - {path}")
        print(f"Update {REQUIRED_CONTROL_FILE} in the same PR.")
        return 2

    print("EFIS project-control check OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
