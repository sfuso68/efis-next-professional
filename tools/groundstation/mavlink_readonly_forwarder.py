#!/usr/bin/env python3
"""Unidirectional Pixhawk serial-to-UDP bridge for the EFIS bench.

The serial descriptor is opened O_RDONLY.  This process has no command path,
does not parse MAVLink and never writes a byte to the flight controller.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import errno
import os
import selectors
import signal
import socket
import sys
import termios
import time
import tty
from typing import Iterable


MAX_UDP_PAYLOAD = 65_507


@dataclass
class Statistics:
    serial_opens: int = 0
    reconnects: int = 0
    serial_bytes: int = 0
    datagrams: int = 0
    udp_bytes: int = 0
    read_errors: int = 0
    send_errors: int = 0


def destination(value: str) -> tuple[str, int]:
    host, separator, port_text = value.rpartition(":")
    if not separator or not host:
        raise argparse.ArgumentTypeError("destination must be HOST:PORT")
    try:
        port = int(port_text)
    except ValueError as error:
        raise argparse.ArgumentTypeError("destination port must be numeric") from error
    if port < 1 or port > 65_535:
        raise argparse.ArgumentTypeError("destination port out of range")
    return host, port


class ReadOnlyForwarder:
    def __init__(
        self,
        serial_path: str,
        destinations: Iterable[tuple[str, int]],
        *,
        read_size: int = 4096,
        report_seconds: float = 5.0,
        retry_seconds: float = 2.0,
    ) -> None:
        if read_size < 1 or read_size > MAX_UDP_PAYLOAD:
            raise ValueError("read_size outside UDP payload range")
        self.serial_path = serial_path
        self.destinations = tuple(destinations)
        if not self.destinations:
            raise ValueError("at least one destination is required")
        self.read_size = read_size
        self.report_seconds = max(0.1, report_seconds)
        self.retry_seconds = max(0.1, retry_seconds)
        self.statistics = Statistics()
        self.running = True

    def stop(self, _signum: int | None = None, _frame: object | None = None) -> None:
        self.running = False

    def _report(self, state: str) -> None:
        stats = self.statistics
        print(
            "event=mavlink_readonly_bridge_health"
            f" state={state} serial_opens={stats.serial_opens}"
            f" reconnects={stats.reconnects} serial_bytes={stats.serial_bytes}"
            f" datagrams={stats.datagrams} udp_bytes={stats.udp_bytes}"
            f" read_errors={stats.read_errors} send_errors={stats.send_errors}",
            flush=True,
        )

    def _forward_session(self, udp: socket.socket, deadline: float | None) -> None:
        # O_RDONLY is the safety boundary: this process cannot write commands,
        # heartbeats, parameters or stream requests to the Pixhawk.
        fd = os.open(self.serial_path, os.O_RDONLY | os.O_NOCTTY | os.O_NONBLOCK)
        original_attributes = termios.tcgetattr(fd)
        selector = selectors.DefaultSelector()
        try:
            tty.setraw(fd, when=termios.TCSANOW)
            selector.register(fd, selectors.EVENT_READ)
            self.statistics.serial_opens += 1
            next_report = time.monotonic()
            while self.running and (deadline is None or time.monotonic() < deadline):
                events = selector.select(timeout=0.25)
                for _key, _mask in events:
                    try:
                        payload = os.read(fd, self.read_size)
                    except BlockingIOError:
                        continue
                    except OSError:
                        self.statistics.read_errors += 1
                        raise
                    if not payload:
                        raise OSError(errno.ENODEV, "serial source disconnected")
                    self.statistics.serial_bytes += len(payload)
                    for target in self.destinations:
                        try:
                            udp.sendto(payload, target)
                            self.statistics.datagrams += 1
                            self.statistics.udp_bytes += len(payload)
                        except OSError:
                            self.statistics.send_errors += 1
                now = time.monotonic()
                if now >= next_report:
                    self._report("LIVE")
                    next_report = now + self.report_seconds
        finally:
            selector.close()
            try:
                termios.tcsetattr(fd, termios.TCSANOW, original_attributes)
            except OSError:
                pass
            os.close(fd)

    def run(self, seconds: float = 0.0) -> int:
        deadline = time.monotonic() + seconds if seconds > 0 else None
        udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        print(
            "event=mavlink_readonly_bridge_start mode=SERIAL_RX_ONLY"
            f" serial={self.serial_path} destinations={len(self.destinations)}",
            flush=True,
        )
        try:
            while self.running and (deadline is None or time.monotonic() < deadline):
                try:
                    self._forward_session(udp, deadline)
                except OSError as error:
                    self.statistics.reconnects += 1
                    print(
                        "event=mavlink_readonly_bridge_serial_unavailable"
                        f" errno={error.errno} reason={error.strerror!r}",
                        flush=True,
                    )
                    if self.running and (deadline is None or time.monotonic() < deadline):
                        time.sleep(self.retry_seconds)
            self._report("STOPPED")
            return 0
        finally:
            udp.close()


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serial", required=True)
    parser.add_argument(
        "--destination", action="append", type=destination, required=True,
        help="repeatable HOST:PORT UDP output",
    )
    parser.add_argument("--read-size", type=int, default=4096)
    parser.add_argument("--report-seconds", type=float, default=5.0)
    parser.add_argument("--retry-seconds", type=float, default=2.0)
    parser.add_argument("--seconds", type=float, default=0.0)
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    forwarder = ReadOnlyForwarder(
        args.serial,
        args.destination,
        read_size=args.read_size,
        report_seconds=args.report_seconds,
        retry_seconds=args.retry_seconds,
    )
    signal.signal(signal.SIGINT, forwarder.stop)
    signal.signal(signal.SIGTERM, forwarder.stop)
    return forwarder.run(args.seconds)


if __name__ == "__main__":
    raise SystemExit(main())
