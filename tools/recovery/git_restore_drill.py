#!/usr/bin/env python3
"""Perform an isolated, fail-closed restore drill from a Git replica.

The tool never replaces an existing working copy.  It clones into a new
directory, verifies repository integrity and records a machine-readable report.
It deliberately does not delete the restored tree so an operator can inspect it.
"""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import os
import pathlib
import re
import subprocess
import sys
from typing import Any


COMMIT_RE = re.compile(r"^[0-9a-fA-F]{40}$")
SECRET_URL_RE = re.compile(r"^[a-z][a-z0-9+.-]*://[^/@:]+:[^/@]+@", re.I)


class DrillError(RuntimeError):
    """A controlled restore-drill failure."""


def load_config(path: pathlib.Path) -> dict[str, Any]:
    data = json.loads(path.read_text(encoding="utf-8"))
    if data.get("schema") != "efis-next.git-restore-drill-config.v1":
        raise DrillError("unsupported restore-drill configuration schema")
    required = data.get("required_paths")
    if not isinstance(required, list) or not required or not all(
            isinstance(item, str) and item for item in required):
        raise DrillError("required_paths must be a non-empty string list")
    return data


def atomic_write(path: pathlib.Path, payload: dict[str, Any]) -> None:
    if path.exists():
        raise DrillError(f"report already exists: {path}")
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(f".{path.name}.tmp-{os.getpid()}")
    try:
        temporary.write_text(
            json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
        os.replace(temporary, path)
    finally:
        if temporary.exists():
            temporary.unlink()


def run(command: list[str], cwd: pathlib.Path | None = None) -> str:
    environment = os.environ.copy()
    environment["GIT_TERMINAL_PROMPT"] = "0"
    environment.setdefault("GIT_SSH_COMMAND", "ssh -o BatchMode=yes")
    completed = subprocess.run(
        command,
        cwd=cwd,
        env=environment,
        check=False,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    if completed.returncode != 0:
        detail = (completed.stderr or completed.stdout).strip()
        raise DrillError(f"command failed ({completed.returncode}): "
                         f"{' '.join(command[:3])}: {detail}")
    return completed.stdout.strip()


def validate_inputs(source: str, destination: pathlib.Path,
                    expected_main: str, report: pathlib.Path) -> None:
    if SECRET_URL_RE.match(source):
        raise DrillError("credentials in source URLs are forbidden")
    if not COMMIT_RE.fullmatch(expected_main):
        raise DrillError("expected main must be a full 40-character commit SHA")
    if destination.exists():
        raise DrillError(f"destination must not exist: {destination}")
    if report.exists():
        raise DrillError(f"report already exists: {report}")
    resolved = destination.resolve(strict=False)
    anchor = pathlib.Path(resolved.anchor)
    if resolved == anchor or resolved == pathlib.Path.home().resolve():
        raise DrillError("unsafe restore destination")


def perform_drill(config: dict[str, Any], source: str, destination: pathlib.Path,
                  expected_main: str, report_path: pathlib.Path,
                  required_tags: list[str], project_check: bool) -> dict[str, Any]:
    validate_inputs(source, destination, expected_main, report_path)
    started = dt.datetime.now(dt.timezone.utc)
    checks: dict[str, bool] = {}
    details: dict[str, Any] = {}
    error = ""

    try:
        destination.parent.mkdir(parents=True, exist_ok=True)
        run(["git", "clone", "--no-hardlinks", "--no-tags", source, str(destination)])
        checks["isolated_clone"] = destination.is_dir()

        run(["git", "fetch", "--force", "--tags", "origin"], cwd=destination)
        run(["git", "fsck", "--full", "--strict"], cwd=destination)
        checks["object_integrity"] = True

        restored_main = run(
            ["git", "rev-parse", "refs/remotes/origin/main^{commit}"], cwd=destination
        ).lower()
        details["restored_main"] = restored_main
        checks["main_matches_expected"] = restored_main == expected_main.lower()

        tags_text = run(
            ["git", "for-each-ref", "--format=%(refname:strip=2)", "refs/tags"],
            cwd=destination,
        )
        tags = sorted(filter(None, tags_text.splitlines()))
        details["tags"] = tags
        details["tag_count"] = len(tags)
        checks["required_tags_present"] = all(tag in tags for tag in required_tags)

        object_lines = run(
            ["git", "rev-list", "--objects", "--all"], cwd=destination
        ).splitlines()
        details["reachable_object_entries"] = len(object_lines)
        checks["reachable_objects_present"] = bool(object_lines)

        missing = [
            item for item in config["required_paths"] if not (destination / item).exists()
        ]
        details["missing_required_paths"] = missing
        checks["required_paths_present"] = not missing

        if project_check:
            generator = destination / "tools" / "project_status" / "generate_status.py"
            run([sys.executable, str(generator), "--check"], cwd=destination)
            checks["project_control_check"] = True
        else:
            checks["project_control_check"] = False
            details["project_control_check"] = "NOT_RUN"
    except (DrillError, OSError) as exc:
        error = str(exc)

    finished = dt.datetime.now(dt.timezone.utc)
    all_required = [
        "isolated_clone", "object_integrity", "main_matches_expected",
        "required_tags_present", "reachable_objects_present",
        "required_paths_present",
    ]
    if project_check:
        all_required.append("project_control_check")
    status = "PASS" if not error and all(checks.get(name) for name in all_required) else "FAIL"
    payload: dict[str, Any] = {
        "schema": "efis-next.git-restore-drill-report.v1",
        "config_sha256": hashlib.sha256(
            json.dumps(config, sort_keys=True, separators=(",", ":")).encode("utf-8")
        ).hexdigest(),
        "status": status,
        "flight_approval": False,
        "started_utc": started.isoformat().replace("+00:00", "Z"),
        "finished_utc": finished.isoformat().replace("+00:00", "Z"),
        "source": source,
        "destination": str(destination.resolve(strict=False)),
        "expected_main": expected_main.lower(),
        "checks": checks,
        "details": details,
        "error": error,
        "restored_tree_retained": destination.exists(),
    }
    atomic_write(report_path, payload)
    return payload


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(description=__doc__)
    result.add_argument(
        "--config", type=pathlib.Path,
        default=pathlib.Path("config/recovery/git-restore-drill-v1.json"),
    )
    result.add_argument("--source", help="Git replica path/URL; defaults to config")
    result.add_argument("--destination", type=pathlib.Path, required=True)
    result.add_argument("--expected-main", required=True)
    result.add_argument("--required-tag", action="append", default=[])
    result.add_argument("--report", type=pathlib.Path, required=True)
    result.add_argument(
        "--project-check", action="store_true",
        help="also run the fixed Project Control generated-status check",
    )
    return result


def main() -> int:
    args = parser().parse_args()
    try:
        config = load_config(args.config)
        source = args.source or config.get("default_source")
        if not isinstance(source, str) or not source:
            raise DrillError("source is required")
        payload = perform_drill(
            config, source, args.destination, args.expected_main,
            args.report, args.required_tag, args.project_check,
        )
    except (DrillError, OSError, json.JSONDecodeError) as exc:
        print(f"restore drill setup failed: {exc}", file=sys.stderr)
        return 2
    print(json.dumps(payload, sort_keys=True))
    return 0 if payload["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
