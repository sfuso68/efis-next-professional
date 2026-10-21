#!/usr/bin/env python3
"""Quarantined legacy parameter writer: no device or network access.

ADR-015 does not authorize bidirectional maintenance. The historical writer
is deliberately unavailable, including dry-run queries: even a parameter
request requires transmission. Legacy flags are parsed only to return an
explicit refusal, never to enable writing. No pymavlink dependency is loaded.

A future maintenance tool requires a separately approved architecture and
interlocks, not an additional CLI override in this module.
"""
from __future__ import annotations

import argparse
import json


class ParameterError(RuntimeError):
    pass


def write_parameter(*args, **kwargs):
    """Fail before inspecting even the supplied link object."""
    raise ParameterError("MAVLink parameter writes are not authorized by ADR-015")


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    # Preserve recognition of historical command lines, not their capabilities.
    for name in ("device", "baud", "name", "expected", "value", "timeout", "retries", "audit-dir"):
        parser.add_argument("--" + name, help=argparse.SUPPRESS)
    parser.add_argument("--commit", action="store_true", help="refused: writer quarantined")
    parser.add_argument("--allow-protected", action="store_true", help="refused: no override exists")
    parser.parse_args(argv)
    print(json.dumps({
        "schema": "efis-next.parameter-write-quarantine.v1",
        "outcome": "refused_not_authorized",
        "reason": "ADR_015_MAINTENANCE_NOT_AUTHORIZED",
        "connection_opened": False,
        "transmitted": False,
        "write_authorized": False,
        "audit_persisted": False,
        "flight_approval": False,
    }, sort_keys=True))
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
