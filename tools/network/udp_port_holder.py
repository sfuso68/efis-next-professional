#!/usr/bin/env python3
"""Hold a UDP endpoint for deterministic collision/fail-closed tests."""

from __future__ import annotations

import argparse
import json
import socket
import time


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bind", default="0.0.0.0")
    parser.add_argument("--port", type=int, required=True)
    parser.add_argument("--duration", type=float, default=60.0)
    args = parser.parse_args()
    if not 1 <= args.port <= 65535:
        parser.error("port must be between 1 and 65535")
    if not 1.0 <= args.duration <= 600.0:
        parser.error("duration must be between 1 and 600 seconds")
    return args


def hold(bind: str, port: int, duration: float) -> None:
    endpoint = f"{bind}:{port}"
    holder = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        holder.bind((bind, port))
        print(
            json.dumps(
                {
                    "event": "udp_port_holder_ready",
                    "endpoint": endpoint,
                    "duration_seconds": duration,
                    "mode": "RX_HOLD_NO_TRANSMIT",
                },
                sort_keys=True,
            ),
            flush=True,
        )
        time.sleep(duration)
    finally:
        holder.close()


def main() -> int:
    args = parse_args()
    hold(args.bind, args.port, args.duration)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
