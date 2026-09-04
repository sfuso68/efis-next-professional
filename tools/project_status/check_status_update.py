#!/usr/bin/env python3
"""Check that subsystem changes are accompanied by project-control updates."""

from __future__ import annotations

import argparse
from pathlib import Path

CONTROL_FILES = {
    "docs/status/EFIS_NEXT_STATE.md",
    "docs/status/EFIS_NEXT_MASTER_MATRIX.md",
}

WATCHED_PREFIXES = (
    "include/efis/",
    "src/core/",
    "src/providers/",
    "src/ingest/",
    "src/io/",
    "src/nodes/",
    "src/instruments/",
    "src/renderers/",
    "qml/",
    "server/",
    "deploy/",
    "config/",
    "tools/terrain/",
    "tools/imagery/",
    "tools/navigation/",
    "docs/hardware/",
    "docs/protocols/",
)

EXEMPT_PREFIXES = (
    "tools/project_status/",
    "docs/status/",
    ".github/",
)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("files", nargs="*", help="changed repository paths")
    parser.add_argument("--file-list", help="newline separated changed paths")
    args = parser.parse_args()

    changed = list(args.files)
    if args.file_list:
        changed.extend(Path(args.file_list).read_text(encoding="utf-8").splitlines())
    changed = [p.strip().replace("\\", "/") for p in changed if p.strip()]

    substantive = [
        p for p in changed
        if p.startswith(WATCHED_PREFIXES) and not p.startswith(EXEMPT_PREFIXES)
    ]
    control_updated = any(p in CONTROL_FILES for p in changed)

    if substantive and not control_updated:
        print("EFIS project-control check FAILED")
        print("Subsystem files changed without updating project status:")
        for path in substantive:
            print(f"  - {path}")
        print("Update docs/status/EFIS_NEXT_MASTER_MATRIX.md or EFIS_NEXT_STATE.md in the same PR.")
        return 2

    print("EFIS project-control check OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
