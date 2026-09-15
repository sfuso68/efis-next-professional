#!/usr/bin/env python3
"""Create a deterministic integrity digest for an extracted QGC AppDir."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import stat


SCHEMA = "efis-next.qgc-appdir-digest.v1"


def _field(digest: "hashlib._Hash", value: str | int) -> None:
    encoded = str(value).encode("utf-8", errors="surrogateescape")
    digest.update(len(encoded).to_bytes(8, "big"))
    digest.update(encoded)


def digest_tree(root: Path) -> dict[str, object]:
    """Hash names, types, executable bits, symlink targets and file contents."""
    root = root.expanduser()
    if not root.is_dir():
        raise ValueError(f"AppDir is missing or not a directory: {root}")

    digest = hashlib.sha256()
    counts = {"files": 0, "symlinks": 0, "directories": 0, "bytes": 0}

    def visit(directory: Path) -> None:
        entries = sorted(os.scandir(directory), key=lambda item: os.fsencode(item.name))
        for entry in entries:
            path = Path(entry.path)
            relative = path.relative_to(root).as_posix()
            metadata = entry.stat(follow_symlinks=False)
            executable = metadata.st_mode & 0o111
            if stat.S_ISLNK(metadata.st_mode):
                _field(digest, "L")
                _field(digest, relative)
                _field(digest, os.readlink(path))
                counts["symlinks"] += 1
            elif stat.S_ISDIR(metadata.st_mode):
                _field(digest, "D")
                _field(digest, relative)
                _field(digest, executable)
                counts["directories"] += 1
                visit(path)
            elif stat.S_ISREG(metadata.st_mode):
                _field(digest, "F")
                _field(digest, relative)
                _field(digest, executable)
                _field(digest, metadata.st_size)
                with path.open("rb") as handle:
                    for chunk in iter(lambda: handle.read(1024 * 1024), b""):
                        digest.update(chunk)
                counts["files"] += 1
                counts["bytes"] += metadata.st_size
            else:
                raise ValueError(f"unsupported AppDir entry: {relative}")

    visit(root)
    return {
        "schema": SCHEMA,
        "root": str(root),
        "tree_sha256": digest.hexdigest(),
        "file_count": counts["files"],
        "symlink_count": counts["symlinks"],
        "directory_count": counts["directories"],
        "file_bytes": counts["bytes"],
    }


def write_report(path: Path, report: dict[str, object]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(
        json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    temporary.replace(path)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        report = digest_tree(args.root)
    except (OSError, ValueError) as error:
        print(json.dumps({"schema": SCHEMA, "status": "FAIL", "error": str(error)}))
        return 1
    if args.output:
        write_report(args.output, report)
    print(json.dumps(report, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
