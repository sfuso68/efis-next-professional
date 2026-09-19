#!/usr/bin/env python3
"""Process-level proof that the Blackbox observer path is source-filtered and TX-only."""

from __future__ import annotations

import socket
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path


BROKER = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else None


def free_udp_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
        probe.bind(("127.0.0.1", 0))
        return int(probe.getsockname()[1])


class BlackboxBrokerObserverTests(unittest.TestCase):
    def broker_command(
        self,
        records: Path,
        *,
        ingress_port: int,
        source: str,
        observer: str,
        observer_port: int,
        display_port: int,
        seconds: int = 2,
    ) -> list[str]:
        return [
            str(BROKER),
            "--mavlink-bind", "127.0.0.1",
            "--mavlink-port", str(ingress_port),
            "--mavlink-source", source,
            "--mavlink-observer", observer,
            "--mavlink-observer-port", str(observer_port),
            "--display", "127.0.0.1",
            "--display-port", str(display_port),
            "--source-node", "47",
            "--records", str(records),
            "--rate-hz", "20",
            "--seconds", str(seconds),
        ]

    def test_only_declared_source_is_mirrored(self) -> None:
        self.assertIsNotNone(BROKER, "broker executable argument is required")
        self.assertTrue(BROKER.is_file(), f"broker executable not found: {BROKER}")
        ingress_port = free_udp_port()
        with tempfile.TemporaryDirectory() as temporary, \
             socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as observer, \
             socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as display:
            observer.bind(("127.0.0.1", 0))
            display.bind(("127.0.0.1", 0))
            observer.settimeout(0.30)
            command = self.broker_command(
                Path(temporary) / "records",
                ingress_port=ingress_port,
                source="127.0.0.2",
                observer="127.0.0.1",
                observer_port=int(observer.getsockname()[1]),
                display_port=int(display.getsockname()[1]),
            )
            process = subprocess.Popen(
                command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                text=True,
            )
            try:
                time.sleep(0.20)
                rejected = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                try:
                    rejected.bind(("127.0.0.3", 0))
                    rejected.sendto(b"rejected-source", ("127.0.0.1", ingress_port))
                finally:
                    rejected.close()
                with self.assertRaises(socket.timeout):
                    observer.recvfrom(65535)

                accepted = b"\xfd\x01\x00\x00\x01\x01\x01\x00\x00\x00\x00\x00\x00"
                with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as source:
                    source.bind(("127.0.0.2", 0))
                    source.sendto(accepted, ("127.0.0.1", ingress_port))
                observer.settimeout(1.5)
                mirrored, _ = observer.recvfrom(65535)
                self.assertEqual(mirrored, accepted)

                output, _ = process.communicate(timeout=5)
                self.assertEqual(process.returncode, 0, output)
                self.assertIn("mavlink_source_rejected=1", output)
                self.assertIn("mavlink_observer_mode=TX_ONLY", output)
                self.assertIn("mavlink_observer_failures=0", output)
            finally:
                if process.poll() is None:
                    process.kill()
                    process.communicate(timeout=2)

    def test_observer_equal_to_source_fails_closed(self) -> None:
        self.assertIsNotNone(BROKER, "broker executable argument is required")
        with tempfile.TemporaryDirectory() as temporary:
            command = self.broker_command(
                Path(temporary) / "records",
                ingress_port=free_udp_port(),
                source="127.0.0.2",
                observer="127.0.0.2",
                observer_port=free_udp_port(),
                display_port=free_udp_port(),
                seconds=1,
            )
            process = subprocess.run(
                command,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                timeout=5,
                check=False,
            )
            self.assertNotEqual(process.returncode, 0, process.stdout)
            self.assertIn("must not equal the protected MAVLink source", process.stdout)

    def test_flight_data_observer_fails_closed(self) -> None:
        self.assertIsNotNone(BROKER, "broker executable argument is required")
        with tempfile.TemporaryDirectory() as temporary:
            command = self.broker_command(
                Path(temporary) / "records",
                ingress_port=free_udp_port(),
                source="127.0.0.2",
                observer="10.77.10.3",
                observer_port=14550,
                display_port=free_udp_port(),
                seconds=1,
            )
            process = subprocess.run(
                command,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                timeout=5,
                check=False,
            )
            self.assertNotEqual(process.returncode, 0, process.stdout)
            self.assertIn("must not be in FLIGHT-DATA", process.stdout)


if __name__ == "__main__":
    # unittest must not interpret the broker path as a test selector.
    unittest.main(argv=[sys.argv[0]])
