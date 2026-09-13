#!/usr/bin/env python3
"""Receive Emergency Assist audit events and acknowledge durable persistence.

The API is deliberately one-way: it accepts audit records and exposes health or
read-only replay data. It has no command, maintenance, MAVLink or actuator API.
"""

import argparse
import hashlib
import hmac
import json
import sqlite3
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse


SCHEMA = "efis-next.emergency-event.v1"
ACK_SCHEMA = "efis-next.emergency-event-ack.v1"


def canonical(value):
    return json.dumps(value, sort_keys=True, separators=(",", ":"))


def event_digest(source, event):
    return hashlib.sha256((source + "\n" + canonical(event)).encode("utf-8")).hexdigest()


def validate_event(event):
    if not isinstance(event, dict):
        raise ValueError("event_must_be_object")
    if event.get("schema") != SCHEMA:
        raise ValueError("unsupported_schema")
    sequence = event.get("event_sequence")
    if not isinstance(sequence, int) or isinstance(sequence, bool) or sequence < 1:
        raise ValueError("invalid_event_sequence")
    for key, limit in (("session_id", 128), ("code", 128), ("assist_state", 64),
                       ("emergency_kind", 64), ("written_utc", 64)):
        value = event.get(key)
        if not isinstance(value, str) or not value or len(value) > limit:
            raise ValueError("invalid_" + key)
    detail = event.get("detail", "")
    if not isinstance(detail, str) or len(detail) > 4096:
        raise ValueError("invalid_detail")
    simulation = event.get("simulation")
    if not isinstance(simulation, bool):
        raise ValueError("invalid_simulation")
    encoded = canonical(event).encode("utf-8")
    if len(encoded) > 32768:
        raise ValueError("event_too_large")
    return event


class SequenceConflict(Exception):
    pass


class EmergencyEventHub:
    def __init__(self, database, tokens):
        self.tokens = tokens
        self.lock = threading.Lock()
        Path(database).parent.mkdir(parents=True, exist_ok=True)
        self.database = database
        self.db = sqlite3.connect(database, check_same_thread=False)
        self.db.execute("PRAGMA journal_mode=WAL")
        self.db.execute("PRAGMA synchronous=FULL")
        self.db.execute(
            "CREATE TABLE IF NOT EXISTS emergency_events("
            "id TEXT PRIMARY KEY, source TEXT NOT NULL, session_id TEXT NOT NULL, "
            "event_sequence INTEGER NOT NULL, received_utc TEXT NOT NULL, "
            "event_utc TEXT NOT NULL, payload TEXT NOT NULL, "
            "UNIQUE(source, session_id, event_sequence))"
        )
        self.db.commit()

    def close(self):
        with self.lock:
            self.db.close()

    def authorized(self, source, token):
        return source in self.tokens and hmac.compare_digest(self.tokens[source], token)

    def reader_authorized(self, token):
        return any(hmac.compare_digest(value, token) for value in self.tokens.values())

    def insert(self, source, event):
        validate_event(event)
        digest = event_digest(source, event)
        session_id = event["session_id"]
        sequence = event["event_sequence"]
        received = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
        payload = canonical(event)
        with self.lock:
            previous = self.db.execute(
                "SELECT id FROM emergency_events WHERE source=? AND session_id=? "
                "AND event_sequence=?", (source, session_id, sequence)
            ).fetchone()
            if previous:
                if previous[0] != digest:
                    raise SequenceConflict("sequence_payload_mismatch")
                return digest, False, received
            self.db.execute(
                "INSERT INTO emergency_events VALUES(?,?,?,?,?,?,?)",
                (digest, source, session_id, sequence, received,
                 event["written_utc"], payload),
            )
            # synchronous=FULL plus commit is the persistence boundary advertised
            # by the acknowledgement. It is not an aircraft power-loss test.
            self.db.commit()
        return digest, True, received

    def count(self):
        with self.lock:
            return self.db.execute("SELECT COUNT(*) FROM emergency_events").fetchone()[0]

    def recent(self, limit):
        with self.lock:
            rows = self.db.execute(
                "SELECT id,source,received_utc,payload FROM emergency_events "
                "ORDER BY rowid DESC LIMIT ?", (limit,)
            ).fetchall()
        return [
            {"id": row[0], "source": row[1], "received_utc": row[2],
             "event": json.loads(row[3])}
            for row in rows
        ]


def make_handler(hub):
    class Handler(BaseHTTPRequestHandler):
        def reply(self, status, data):
            body = canonical(data).encode("utf-8")
            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def bearer(self):
            return self.headers.get("Authorization", "").removeprefix("Bearer ")

        def do_GET(self):
            parsed = urlparse(self.path)
            if parsed.path == "/health":
                return self.reply(200, {
                    "ok": True,
                    "schema": "efis-next.emergency-event-hub-health.v1",
                    "records": hub.count(),
                    "command_api": False,
                })
            if parsed.path == "/v1/emergency-events":
                if not hub.reader_authorized(self.bearer()):
                    return self.reply(401, {"error": "unauthorized"})
                try:
                    limit = min(500, max(1, int(parse_qs(parsed.query).get("limit", [50])[0])))
                except ValueError:
                    return self.reply(400, {"error": "invalid_limit"})
                return self.reply(200, {"events": hub.recent(limit)})
            return self.reply(404, {"error": "not_found"})

        def do_POST(self):
            if self.path != "/v1/emergency-events":
                return self.reply(404, {"error": "not_found"})
            try:
                length = int(self.headers.get("Content-Length", "0"))
                if length < 1 or length > 65536:
                    raise ValueError("invalid_content_length")
                data = json.loads(self.rfile.read(length))
                source = str(data["source"])
                if not hub.authorized(source, self.bearer()):
                    return self.reply(401, {"error": "unauthorized"})
                digest, inserted, received = hub.insert(source, data["event"])
                return self.reply(201 if inserted else 200, {
                    "schema": ACK_SCHEMA,
                    "id": digest,
                    "source": source,
                    "session_id": data["event"]["session_id"],
                    "event_sequence": data["event"]["event_sequence"],
                    "inserted": inserted,
                    "persisted": True,
                    "received_utc": received,
                })
            except SequenceConflict as exc:
                return self.reply(409, {"error": "sequence_conflict", "detail": str(exc)})
            except (KeyError, TypeError, ValueError, json.JSONDecodeError) as exc:
                return self.reply(400, {"error": "bad_request", "detail": str(exc)})

        def log_message(self, fmt, *args):
            pass

    return Handler


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--bind", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=18094)
    parser.add_argument("--database", required=True)
    parser.add_argument("--tokens", required=True)
    args = parser.parse_args()
    tokens = json.loads(Path(args.tokens).read_text(encoding="utf-8"))
    hub = EmergencyEventHub(args.database, tokens)
    ThreadingHTTPServer((args.bind, args.port), make_handler(hub)).serve_forever()


if __name__ == "__main__":
    main()
