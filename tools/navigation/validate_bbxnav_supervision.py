#!/usr/bin/env python3
"""Validate the fail-closed systemd supervision policy for legacy bbxnav."""

from __future__ import annotations

import argparse
import configparser
import json
import sys
from pathlib import Path


DEFAULT_OVERRIDE = Path("deploy/systemd/bbxnav-fail-closed.override.conf")


class PolicyError(ValueError):
    """The supervision policy is missing a required fail-closed property."""


def load_policy(path: Path) -> configparser.ConfigParser:
    parser = configparser.ConfigParser(interpolation=None, strict=True)
    parser.optionxform = str
    try:
        with path.open("r", encoding="utf-8") as stream:
            parser.read_file(stream)
    except (OSError, configparser.Error) as exc:
        raise PolicyError(f"cannot parse {path}: {exc}") from exc
    return parser


def positive_integer(value: str, field: str) -> int:
    try:
        parsed = int(value)
    except ValueError as exc:
        raise PolicyError(f"{field} must be an integer") from exc
    if parsed <= 0:
        raise PolicyError(f"{field} must be greater than zero")
    return parsed


def validate_policy(path: Path) -> dict[str, object]:
    policy = load_policy(path)
    if set(policy.sections()) != {"Unit", "Service"}:
        raise PolicyError("override must contain exactly Unit and Service sections")

    unit = policy["Unit"]
    service = policy["Service"]
    allowed_unit = {"StartLimitIntervalSec", "StartLimitBurst"}
    allowed_service = {"Restart", "RestartPreventExitStatus", "RestartSec"}
    if set(unit) != allowed_unit:
        raise PolicyError("Unit section contains missing or unexpected directives")
    if set(service) != allowed_service:
        raise PolicyError("Service section contains missing or unexpected directives")

    if service["Restart"] != "on-failure":
        raise PolicyError("Restart must be on-failure")
    prevented = service["RestartPreventExitStatus"].split()
    if prevented != ["2"]:
        raise PolicyError("exit status 2 must be the sole non-restart condition")
    if unit["StartLimitIntervalSec"] in {"0", "0s", "infinity"}:
        raise PolicyError("StartLimitIntervalSec must be finite and non-zero")
    positive_integer(unit["StartLimitBurst"], "StartLimitBurst")
    if service["RestartSec"] in {"0", "0s"}:
        raise PolicyError("RestartSec must be non-zero")

    # A supervision-only drop-in must never alter command, user, capabilities,
    # CAN interface or data source. Those belong to a separately reviewed
    # release and hardware authorization.
    forbidden_prefixes = ("Exec", "User", "Group", "Environment", "Capability")
    for section in policy.sections():
        for directive in policy[section]:
            if directive.startswith(forbidden_prefixes):
                raise PolicyError(f"forbidden directive in supervision override: {directive}")

    return {
        "schema": "efis-next.bbxnav-supervision-validation.v1",
        "path": str(path),
        "restart": service["Restart"],
        "restart_prevent_exit_status": prevented,
        "start_limit_burst": positive_integer(unit["StartLimitBurst"], "StartLimitBurst"),
        "status": "PASS",
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--override", type=Path, default=DEFAULT_OVERRIDE)
    args = parser.parse_args()
    try:
        result = validate_policy(args.override)
    except PolicyError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
