#!/usr/bin/env python3
"""Validate a completed imagery cache and atomically publish its active package."""
import argparse
import json
import os
import tempfile
from datetime import datetime, timezone
from pathlib import Path


def valid_image(path):
    try:
        with path.open("rb") as stream:
            signature = stream.read(12)
        return ((signature.startswith(b"RIFF") and signature[8:12] == b"WEBP")
                or signature.startswith(b"\x89PNG"))
    except OSError:
        return False


def build_status(plan, report, cache_root):
    expected = {entry.get("path") or entry.get("cache_path") for entry in plan["tiles"]}
    expected.discard(None)
    missing = sorted(path for path in expected if not valid_image(cache_root / path))
    report_ok = (report.get("failed") == 0 and report.get("requested") == len(expected))
    ready = report_ok and not missing
    return {
        "schema": "efis-next.active-imagery-package.v1",
        "package_id": plan.get("package_id", plan.get("selection_id", "unknown")),
        "dataset_id": plan["dataset_id"],
        "selection_mode": plan.get("selection_mode"),
        "state": "OFFLINE_READY" if ready else "INCOMPLETE",
        "offline_ready": ready,
        "verified_utc": datetime.now(timezone.utc).isoformat(),
        "cache_root": str(cache_root.resolve()),
        "tile_count": len(expected),
        "verified_tile_count": len(expected) - len(missing),
        "bytes": report.get("bytes", 0),
        "coverage_complete": plan.get("coverage_complete", False),
        "coverage_gap_tile_count": plan.get("coverage_gap_tile_count", 0),
        "missing": missing[:100],
    }


def atomic_json(path, payload):
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary_name = tempfile.mkstemp(prefix=path.name + ".", dir=path.parent)
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8") as stream:
            json.dump(payload, stream, indent=2)
            stream.write("\n")
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary_name, path)
    finally:
        Path(temporary_name).unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--plan", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--cache", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    status = build_status(json.loads(args.plan.read_text()), json.loads(args.report.read_text()), args.cache)
    atomic_json(args.output, status)
    print(json.dumps(status))
    if not status["offline_ready"]:
        raise SystemExit(2)


if __name__ == "__main__":
    main()
