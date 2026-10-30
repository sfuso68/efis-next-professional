"""Bounded SIL stdin/stdout source. No socket, actuator, filesystem control API."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import select
import sys
import time
from tools.simulation.jsbsim_session import run_session, strict_json
from tools.simulation.sil_world import config_digest

SCHEMA = "fuso-avionics.sil-live.v1"


def bounded_line(stream, seconds=2):
    """POSIX pipes: one deadline covers partial lines, not just the first byte."""
    deadline = time.monotonic() + seconds
    line = bytearray()
    while True:
        remaining = deadline - time.monotonic()
        if remaining <= 0 or not select.select([stream.fileno()], [], [], remaining)[0]:
            raise ValueError("live request timeout")
        # Single-byte reads avoid consuming the next request; tiny request (<160B).
        byte = os.read(stream.fileno(), 1)
        if not byte:
            raise ValueError("live request EOF")
        line += byte
        if len(line) > 4096:
            raise ValueError("oversize live request")
        if byte == b"\n":
            return bytes(line)


class Exchange:
    def __init__(self, config, output=None, read=None):
        digest = config_digest(config)
        self.canonical = json.dumps(config, sort_keys=True, separators=(",", ":"), allow_nan=False)
        if hashlib.sha256(self.canonical.encode()).hexdigest() != digest:
            raise ValueError("world canonical mismatch")
        self.digest = digest
        self.output = output or sys.stdout
        self.read = read or (lambda: bounded_line(sys.stdin.buffer))
        self.seen = set()
        self.next_sequence = 0
        self.pending = None

    def emit(self, message):
        line = json.dumps(message, sort_keys=True, separators=(",", ":"), allow_nan=False)
        if len(line.encode()) > 65535:
            raise ValueError("oversize live response")
        self.output.write(line + "\n")
        self.output.flush()

    def start(self, session_id, seconds):
        if type(seconds) is not int or not 1 <= seconds <= 120:
            raise ValueError("live bench limited to 120 seconds")
        self.emit({"schema": SCHEMA, "kind": "HELLO", "session_id": session_id,
                   "seconds": seconds, "simulated": True, "flight_approval": False,
                   "world_config_canonical": self.canonical, "world_config_sha256": self.digest})

    def request(self, sequence):
        if sequence != self.next_sequence or self.pending is not None:
            raise ValueError("live exchange order")
        request = strict_json(self.read())
        if (not isinstance(request, dict) or set(request) != {"schema", "sequence", "nonce"}
                or request["schema"] != SCHEMA or type(request["sequence"]) is not int
                or request["sequence"] != sequence or not isinstance(request["nonce"], str)
                or not re.fullmatch(r"[a-f0-9]{32}", request["nonce"])
                or request["nonce"] in self.seen):
            raise ValueError("live request identity/sequence/nonce")
        self.pending = request["nonce"]
        self.seen.add(self.pending)

    def send(self, frame):
        if (self.pending is None or frame["sequence"] != self.next_sequence
                or frame["simulated"] is not True or frame["flight_approval"] is not False):
            raise ValueError("live frame identity/order")
        self.emit({"schema": SCHEMA, "kind": "FRAME", "nonce": self.pending, "frame": frame})
        self.pending = None
        self.next_sequence += 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--world", required=True, type=Path)
    parser.add_argument("--directory", required=True, type=Path)
    parser.add_argument("--scenario", choices=("cruise", "turn", "engine-out", "sensor-loss"), default="turn")
    parser.add_argument("--seconds", type=int, default=60, choices=range(1, 121))
    args = parser.parse_args()
    try:
        if args.world.stat().st_size > 65536:
            raise ValueError("world too large")
        config = strict_json(args.world.read_text(encoding="utf-8"))
        run_session(args.directory, args.scenario, args.seconds, realtime=True,
                    world_config=config, exchange=Exchange(config))
    except (OSError, ValueError, RuntimeError, ImportError) as exc:
        parser.exit(2, f"SIL live failed: {exc}\n")


if __name__ == "__main__":
    main()
