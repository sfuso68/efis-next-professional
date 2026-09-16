#!/usr/bin/env python3
"""Forward a local Emergency Assist JSONL spool with persistence receipts."""

import argparse
import json
import os
import time
import urllib.error
import urllib.request
from pathlib import Path

import emergency_event_hub as protocol


def atomic_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    with temporary.open("w", encoding="utf-8") as stream:
        json.dump(value, stream, sort_keys=True, separators=(",", ":"))
        stream.flush()
        os.fsync(stream.fileno())
    os.replace(temporary, path)


def load_state(path):
    if not path.exists():
        return {"schema": "efis-next.emergency-forwarder-state.v1", "offset": 0,
                "last_ack_id": ""}
    value = json.loads(path.read_text(encoding="utf-8"))
    if value.get("schema") != "efis-next.emergency-forwarder-state.v1":
        raise ValueError("unsupported_forwarder_state")
    if not isinstance(value.get("offset"), int) or value["offset"] < 0:
        raise ValueError("invalid_forwarder_offset")
    return value


def post_event(url, source, token, event):
    protocol.validate_event(event)
    body = json.dumps({"source": source, "event": event},
                      separators=(",", ":")).encode("utf-8")
    request = urllib.request.Request(
        url.rstrip("/") + "/v1/emergency-events",
        body,
        {"Content-Type": "application/json", "Authorization": "Bearer " + token},
        method="POST",
    )
    with urllib.request.urlopen(request, timeout=5) as response:
        acknowledgement = json.load(response)
    expected = protocol.event_digest(source, event)
    if (acknowledgement.get("schema") != protocol.ACK_SCHEMA or
            acknowledgement.get("id") != expected or
            acknowledgement.get("persisted") is not True or
            acknowledgement.get("source") != source or
            acknowledgement.get("session_id") != event["session_id"] or
            acknowledgement.get("event_sequence") != event["event_sequence"]):
        raise ValueError("invalid_persistence_acknowledgement")
    return acknowledgement


def forward_once(journal, state_path, url, source, token):
    state = load_state(state_path)
    offset = state["offset"]
    if not journal.exists():
        return 0, state
    size = journal.stat().st_size
    if offset > size:
        raise ValueError("journal_truncated_before_acknowledged_offset")

    sent = 0
    with journal.open("rb") as stream:
        stream.seek(offset)
        while True:
            line_start = stream.tell()
            line = stream.readline()
            if not line:
                break
            if not line.endswith(b"\n"):
                # The writer may still be appending this record. Never advance.
                stream.seek(line_start)
                break
            event = json.loads(line)
            acknowledgement = post_event(url, source, token, event)
            offset = stream.tell()
            state = {
                "schema": "efis-next.emergency-forwarder-state.v1",
                "offset": offset,
                "last_ack_id": acknowledgement["id"],
                "last_ack_utc": acknowledgement["received_utc"],
                "last_session_id": event["session_id"],
                "last_event_sequence": event["event_sequence"],
            }
            atomic_json(state_path, state)
            sent += 1
    return sent, state


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--journal", required=True)
    parser.add_argument("--state", required=True)
    parser.add_argument("--url", required=True)
    parser.add_argument("--source", required=True)
    parser.add_argument("--token-file", required=True)
    parser.add_argument("--once", action="store_true")
    args = parser.parse_args()
    journal = Path(args.journal)
    state = Path(args.state)
    token = Path(args.token_file).read_text(encoding="utf-8").strip()
    while True:
        try:
            sent, current = forward_once(journal, state, args.url, args.source, token)
            if args.once:
                print(json.dumps({"sent": sent, "state": current}, sort_keys=True))
                return
        except (OSError, ValueError, urllib.error.URLError, json.JSONDecodeError) as exc:
            print("emergency_forwarder_error", type(exc).__name__, str(exc), flush=True)
            if args.once:
                raise SystemExit(2)
        time.sleep(2)


if __name__ == "__main__":
    main()
