#!/usr/bin/env python3
"""Analyze Blackbox health journal lines for MAVLink observer soak gates."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys
from typing import Iterable


EVENTS = {
    "blackbox_broker_health",
    "blackbox_mavlink_observer_health",
    "blackbox_mavlink_observer_target",
    "blackbox_display_health",
}


def parse_lines(lines: Iterable[str]) -> list[dict[str, str]]:
    records: list[dict[str, str]] = []
    for line in lines:
        fields: dict[str, str] = {}
        for token in line.strip().split():
            if "=" not in token:
                continue
            key, value = token.split("=", 1)
            fields[key] = value
        if fields.get("event") in EVENTS:
            records.append(fields)
    return records


def integer(record: dict[str, str], key: str) -> int:
    try:
        return int(record[key])
    except (KeyError, ValueError) as error:
        raise ValueError(f"missing or invalid {key}") from error


def summarize(records: list[dict[str, str]], key: str) -> dict[str, object]:
    values = [integer(record, key) for record in records]
    monotonic = all(current >= previous for previous, current in zip(values, values[1:]))
    return {
        "samples": len(values),
        "start": values[0] if values else None,
        "end": values[-1] if values else None,
        "delta": values[-1] - values[0] if len(values) >= 2 else 0,
        "monotonic": monotonic,
    }


def analyze(
    records: list[dict[str, str]],
    *,
    duration_seconds: float,
    observers: list[str],
    displays: list[str],
    minimum_mavlink_hz: float,
) -> dict[str, object]:
    if duration_seconds <= 0:
        raise ValueError("duration must be positive")

    broker = [item for item in records if item["event"] == "blackbox_broker_health"]
    fanout = [
        item for item in records if item["event"] == "blackbox_mavlink_observer_health"
    ]
    observer_series = {
        endpoint: [
            item
            for item in records
            if item["event"] == "blackbox_mavlink_observer_target"
            and item.get("destination") == endpoint
        ]
        for endpoint in observers
    }
    display_series = {
        endpoint: [
            item
            for item in records
            if item["event"] == "blackbox_display_health"
            and item.get("destination") == endpoint
        ]
        for endpoint in displays
    }

    mavlink = summarize(broker, "mavlink_datagrams")
    recorded = summarize(broker, "recorded")
    ddp = summarize(broker, "ddp_datagrams")
    fanout_datagrams = summarize(fanout, "datagrams")
    observer_summaries = {
        endpoint: summarize(series, "datagrams")
        for endpoint, series in observer_series.items()
    }
    display_summaries = {
        endpoint: summarize(series, "datagrams")
        for endpoint, series in display_series.items()
    }
    mavlink_rate = float(mavlink["delta"]) / duration_seconds

    checks: dict[str, bool] = {
        "broker_has_multiple_samples": len(broker) >= 2,
        "broker_counters_monotonic": all(
            bool(item["monotonic"]) for item in (mavlink, recorded, ddp)
        ),
        "mavlink_progress": int(mavlink["delta"]) > 0,
        "mavlink_rate": mavlink_rate >= minimum_mavlink_hz,
        "recorder_progress": int(recorded["delta"]) > 0,
        "ddp_progress": int(ddp["delta"]) > 0,
        "broker_errors_zero": all(
            integer(item, key) == 0
            for item in broker
            for key in (
                "mavlink_socket_errors",
                "mavlink_truncated",
                "mavlink_source_rejected",
                "ddp_failures",
            )
        ),
        "fanout_progress": int(fanout_datagrams["delta"]) > 0,
        "fanout_failures_zero": len(fanout) >= 2
        and all(integer(item, "failures") == 0 for item in fanout),
    }
    for endpoint, series in observer_series.items():
        summary = observer_summaries[endpoint]
        checks[f"observer_{endpoint}_progress"] = (
            len(series) >= 2
            and bool(summary["monotonic"])
            and int(summary["delta"]) > 0
        )
        checks[f"observer_{endpoint}_failures_zero"] = len(series) >= 2 and all(
            integer(item, "failures") == 0 for item in series
        )
    for endpoint, series in display_series.items():
        summary = display_summaries[endpoint]
        checks[f"display_{endpoint}_progress"] = (
            len(series) >= 2
            and bool(summary["monotonic"])
            and int(summary["delta"]) > 0
        )
        checks[f"display_{endpoint}_failures_zero"] = len(series) >= 2 and all(
            integer(item, "failures") == 0 for item in series
        )

    return {
        "schema": "efis-next.mavlink-observer-soak.v1",
        "status": "PASS" if all(checks.values()) else "FAIL",
        "duration_seconds": duration_seconds,
        "minimum_mavlink_hz": minimum_mavlink_hz,
        "measured_mavlink_hz": round(mavlink_rate, 3),
        "checks": checks,
        "broker": {
            "mavlink": mavlink,
            "recorded": recorded,
            "ddp": ddp,
        },
        "fanout": fanout_datagrams,
        "observers": observer_summaries,
        "displays": display_summaries,
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", default="-")
    parser.add_argument("--duration-seconds", type=float, required=True)
    parser.add_argument("--observer", action="append", required=True)
    parser.add_argument("--display", action="append", required=True)
    parser.add_argument("--minimum-mavlink-hz", type=float, default=5.0)
    parser.add_argument("--output", type=Path)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.input == "-":
        lines = sys.stdin
    else:
        lines = Path(args.input).open(encoding="utf-8")
    try:
        report = analyze(
            parse_lines(lines),
            duration_seconds=args.duration_seconds,
            observers=args.observer,
            displays=args.display,
            minimum_mavlink_hz=args.minimum_mavlink_hz,
        )
    finally:
        if args.input != "-":
            lines.close()
    document = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.write_text(document, encoding="utf-8")
    print(document, end="")
    return 0 if report["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
