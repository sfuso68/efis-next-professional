#!/usr/bin/env python3
"""Validate a packaged product evidence bundle and every referenced byte."""

from __future__ import annotations

import argparse
from datetime import datetime
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import sys

try:
    from .package_product_evidence import DOMAIN_SPECS
except ImportError:
    from package_product_evidence import DOMAIN_SPECS


ROOT = Path(__file__).resolve().parents[2]
DIGEST = re.compile(r"[0-9a-f]{64}")
BUNDLE_KEYS = {"schema", "domain", "status", "operator", "captured_utc", "source", "subjects", "global_artifacts", "limitations", "review_status", "reviewers", "hardware_verified", "deployment_allowed", "flight_approval", "decision"}
SOURCE_KEYS = {"intake_path", "intake_sha256", "review_gate_path", "review_gate_sha256"}
SUBJECT_KEYS = {"id", "artifacts"}
ARTIFACT_KEYS = {"kind", "file", "bytes", "sha256"}


class BundleValidationError(ValueError):
    """The bundle is incomplete, modified, unsafe, or claims excess authority."""


def require(condition: bool, control: str, message: str) -> None:
    if not condition:
        raise BundleValidationError(f"{control}: {message}")


def _utc(value: object) -> bool:
    try:
        parsed = datetime.fromisoformat(str(value).replace("Z", "+00:00"))
        return parsed.utcoffset() is not None and parsed.utcoffset().total_seconds() == 0
    except (TypeError, ValueError):
        return False


def _safe_file(base: Path, raw: object) -> Path | None:
    if not isinstance(raw, str) or not raw.strip():
        return None
    relative = Path(raw)
    if relative.is_absolute() or ".." in relative.parts:
        return None
    candidate = base / relative
    resolved = candidate.resolve()
    if base.resolve() not in resolved.parents or candidate.is_symlink() or not resolved.is_file():
        return None
    return resolved


def _repo_file(root: Path, raw: object) -> Path | None:
    if not isinstance(raw, str):
        return None
    relative = PurePosixPath(raw)
    if relative.is_absolute() or ".." in relative.parts:
        return None
    candidate = root.resolve().joinpath(*relative.parts)
    resolved = candidate.resolve()
    if root.resolve() not in resolved.parents or candidate.is_symlink() or not resolved.is_file():
        return None
    return resolved


def load_bundle(path: Path) -> dict:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise BundleValidationError("bundle root must be an object")
    return value


def _validate_artifacts(rows: object, kinds: list[str], base: Path, control: str) -> None:
    require(isinstance(rows, list), control, "artifact collection must be an array")
    require([row.get("kind") for row in rows if isinstance(row, dict)] == kinds, control, "artifact kinds or order differ from the frozen contract")
    for row in rows:
        require(isinstance(row, dict) and set(row) == ARTIFACT_KEYS, control, "artifact properties differ from the closed schema")
        path = _safe_file(base, row.get("file"))
        require(path is not None, control, f"{row.get('kind')} artifact is missing or unsafe")
        payload = path.read_bytes()
        require(isinstance(row.get("bytes"), int) and not isinstance(row.get("bytes"), bool) and row["bytes"] == len(payload) and len(payload) > 0, control, f"{row['kind']} size mismatch")
        require(DIGEST.fullmatch(str(row.get("sha256", ""))) is not None and row["sha256"] == hashlib.sha256(payload).hexdigest(), control, f"{row['kind']} digest mismatch")


def validate_bundle(data: dict, root: Path, bundle_dir: Path) -> dict:
    require(data.get("schema") == "avionix-next.product-evidence-bundle.v1", "PEB-01", "wrong bundle schema")
    require(set(data) == BUNDLE_KEYS, "PEB-02", "bundle properties differ from the closed schema")
    domain = data.get("domain")
    require(domain in DOMAIN_SPECS, "PEB-03", "unknown product domain")
    spec = DOMAIN_SPECS[domain]
    require(data.get("status") == "PACKAGED_PENDING_REVIEW", "PEB-04", "bundle is not pending independent review")
    require(isinstance(data.get("operator"), str) and data["operator"].strip() and _utc(data.get("captured_utc")), "PEB-05", "operator or UTC capture timestamp is invalid")
    source = data.get("source")
    require(isinstance(source, dict) and set(source) == SOURCE_KEYS, "PEB-06", "source properties differ from the closed schema")
    for path_key, digest_key, expected in (
        ("intake_path", "intake_sha256", spec["intake"]),
        ("review_gate_path", "review_gate_sha256", spec["gate"]),
    ):
        target = _repo_file(root, source.get(path_key))
        require(source.get(path_key) == expected and target is not None, "PEB-07", f"{path_key} is missing, unsafe or unexpected")
        require(DIGEST.fullmatch(str(source.get(digest_key, ""))) is not None and source[digest_key] == hashlib.sha256(target.read_bytes()).hexdigest(), "PEB-08", f"{digest_key} is malformed or stale")
    subjects = data.get("subjects")
    require(isinstance(subjects, list) and [row.get("id") for row in subjects if isinstance(row, dict)] == spec["subjects"], "PEB-09", "subjects differ from the frozen domain set")
    for row in subjects:
        require(isinstance(row, dict) and set(row) == SUBJECT_KEYS, "PEB-10", "subject properties differ from the closed schema")
        _validate_artifacts(row["artifacts"], spec["artifacts"], bundle_dir, "PEB-11")
    _validate_artifacts(data.get("global_artifacts"), spec["global"], bundle_dir, "PEB-12")
    require(isinstance(data.get("limitations"), list) and all(isinstance(item, str) and item.strip() for item in data["limitations"]), "PEB-13", "limitations are invalid")
    require(data.get("review_status") == "PENDING" and data.get("reviewers") == [], "PEB-14", "review is claimed inside an unreviewed bundle")
    require(data.get("hardware_verified") is False and data.get("deployment_allowed") is False and data.get("flight_approval") is False, "PEB-15", "bundle claims forbidden authority")
    require(data.get("decision") == "NO_GO_PENDING_INDEPENDENT_REVIEW", "PEB-16", "bundle decision is not fail-closed")
    return {
        "schema": "avionix-next.product-evidence-bundle-validation.v1",
        "status": "PASS",
        "domain": domain,
        "subjects": len(subjects),
        "review_status": "PENDING",
        "hardware_verified": False,
        "deployment_allowed": False,
        "flight_approval": False,
        "decision": "NO_GO_PENDING_INDEPENDENT_REVIEW",
    }


def validate_file(root: Path, path: Path) -> dict:
    return validate_bundle(load_bundle(path), root, path.resolve().parent)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--bundle", required=True, type=Path)
    args = parser.parse_args()
    try:
        report = validate_file(args.root, args.bundle)
    except (BundleValidationError, KeyError, TypeError, OSError, json.JSONDecodeError) as exc:
        print(f"AVIONIX product evidence bundle: FAIL: {exc}", file=sys.stderr)
        return 1
    print(json.dumps(report, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
