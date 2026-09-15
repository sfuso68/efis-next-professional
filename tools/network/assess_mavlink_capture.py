#!/usr/bin/env python3
"""Classify a passive MAVLink report without replaying or exposing payload."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import sys


sys.path.insert(0, str(Path(__file__).resolve().parent))
from capture_mavlink_observer import assessment  # noqa: E402


SCHEMA = "efis-next.mavlink-capture-assessment.v1"
SUPPORTED_REPORT_SCHEMAS = {
    "efis-next.mavlink-passive-capture.v2",
    "efis-next.mavlink-passive-capture.v3",
}
MAX_REPORT_BYTES = 1_048_576


class ReportError(ValueError):
    pass


def load_report(path: Path) -> tuple[dict[str, object], str]:
    raw = path.read_bytes()
    if not raw or len(raw) > MAX_REPORT_BYTES:
        raise ReportError("report size is outside the accepted range")
    try:
        document = json.loads(raw)
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise ReportError(f"invalid JSON report: {error}") from error
    if not isinstance(document, dict):
        raise ReportError("report root must be an object")
    if document.get("schema") not in SUPPORTED_REPORT_SCHEMAS:
        raise ReportError("unsupported passive capture schema")
    checks = document.get("checks")
    if not isinstance(checks, dict) or not all(
        isinstance(key, str) and isinstance(value, bool)
        for key, value in checks.items()
    ):
        raise ReportError("checks must be a boolean object")
    return document, hashlib.sha256(raw).hexdigest()


def classify(document: dict[str, object], digest: str) -> dict[str, object]:
    checks = document["checks"]
    assert isinstance(checks, dict)
    result = assessment(checks)
    return {
        "schema": SCHEMA,
        "source_schema": document["schema"],
        "source_sha256": digest,
        "source_status": document.get("status"),
        "assessment": result,
    }


def exit_code(result: dict[str, object]) -> int:
    value = result["assessment"]
    assert isinstance(value, dict)
    if value["link_status"] != "PASS":
        return 1
    if value["quality_status"] != "PASS":
        return 2
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("report", type=Path)
    args = parser.parse_args()
    try:
        document, digest = load_report(args.report)
        result = classify(document, digest)
    except (OSError, ReportError) as error:
        print(f"assessment failed: {error}", file=sys.stderr)
        return 1
    print(json.dumps(result, indent=2, sort_keys=True))
    return exit_code(result)


if __name__ == "__main__":
    raise SystemExit(main())
