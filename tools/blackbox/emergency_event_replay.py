#!/usr/bin/env python3
"""Validate and summarize an Emergency Assist event replay stream."""

import argparse
import json
from collections import defaultdict
from pathlib import Path

import emergency_event_hub as protocol


def load_records(path):
    records = []
    for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not line.strip():
            continue
        value = json.loads(line)
        event = value.get("event", value)
        protocol.validate_event(event)
        records.append((line_number, event))
    return records


def inspect(records):
    sessions = defaultdict(list)
    for line_number, event in records:
        sessions[event["session_id"]].append((line_number, event))
    gaps = []
    regressions = []
    kinds = defaultdict(int)
    simulation_records = 0
    context_records = 0
    for session_id, values in sessions.items():
        previous = None
        for line_number, event in values:
            sequence = event["event_sequence"]
            kinds[event["emergency_kind"]] += 1
            simulation_records += int(event["simulation"])
            context_records += int(isinstance(event.get("decision_context"), dict) and
                                   isinstance(event.get("assist_output"), dict))
            if previous is not None:
                if sequence <= previous:
                    regressions.append({"session_id": session_id, "line": line_number,
                                        "previous": previous, "current": sequence})
                elif sequence != previous + 1:
                    gaps.append({"session_id": session_id, "line": line_number,
                                 "expected": previous + 1, "current": sequence})
            previous = sequence
    return {
        "schema": "efis-next.emergency-replay-report.v1",
        "records": len(records),
        "sessions": len(sessions),
        "simulation_records": simulation_records,
        "decision_context_records": context_records,
        "complete_decision_context": context_records == len(records),
        "kinds": dict(sorted(kinds.items())),
        "sequence_gaps": gaps,
        "sequence_regressions": regressions,
        "valid": not gaps and not regressions,
        "advisory_only": True,
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True)
    parser.add_argument("--output")
    args = parser.parse_args()
    report = inspect(load_records(Path(args.input)))
    encoded = json.dumps(report, sort_keys=True, indent=2)
    if args.output:
        Path(args.output).write_text(encoded + "\n", encoding="utf-8")
    print(encoded)
    raise SystemExit(0 if report["valid"] else 2)


if __name__ == "__main__":
    main()
