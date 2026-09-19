from __future__ import annotations

import importlib.util
import os
from pathlib import Path
import socket
import sys
import threading
import time
import unittest

try:
    import pty
    import tty
except ImportError:
    pty = None
    tty = None


SCRIPT = Path(__file__).parents[1] / "tools" / "groundstation" / "mavlink_readonly_forwarder.py"
SPEC = importlib.util.spec_from_file_location("mavlink_readonly_forwarder", SCRIPT)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class ReadOnlyForwarderTests(unittest.TestCase):
    def test_destination_validation(self) -> None:
        self.assertEqual(MODULE.destination("127.0.0.1:14550"), ("127.0.0.1", 14550))
        with self.assertRaises(Exception):
            MODULE.destination("127.0.0.1")
        with self.assertRaises(Exception):
            MODULE.destination("127.0.0.1:0")
        with self.assertRaises(Exception):
            MODULE.destination("pixhawk.local:14550")
        with self.assertRaises(Exception):
            MODULE.destination("239.1.2.3:14550")
        with self.assertRaises(Exception):
            MODULE.destination("192.168.1.255:14550")

    def test_flight_data_destinations_are_rejected(self) -> None:
        for address in ("10.77.10.2", "10.77.10.3", "10.77.10.11", "10.77.10.254"):
            with self.subTest(address=address):
                with self.assertRaises(Exception):
                    MODULE.destination(f"{address}:14550")
        self.assertEqual(
            MODULE.destination("10.77.11.3:14550"), ("10.77.11.3", 14550)
        )

    def test_direct_construction_revalidates_and_rejects_duplicates(self) -> None:
        with self.assertRaises(ValueError):
            MODULE.ReadOnlyForwarder(
                "/dev/null", [("10.77.10.3", 14550)]
            )
        with self.assertRaises(ValueError):
            MODULE.ReadOnlyForwarder(
                "/dev/null",
                [("127.0.0.1", 14550), ("127.0.0.1", 14550)],
            )

    @unittest.skipUnless(pty is not None and tty is not None, "requires POSIX pty")
    def test_serial_bytes_are_forwarded_unchanged(self) -> None:
        receiver = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        receiver.bind(("127.0.0.1", 0))
        receiver.settimeout(2.0)
        master, slave = pty.openpty()
        tty.setraw(slave)
        forwarder = MODULE.ReadOnlyForwarder(
            os.ttyname(slave), [receiver.getsockname()], report_seconds=60.0
        )
        thread = threading.Thread(target=forwarder.run, kwargs={"seconds": 1.0})
        thread.start()
        open_deadline = time.monotonic() + 1.0
        while forwarder.statistics.serial_opens == 0 and time.monotonic() < open_deadline:
            time.sleep(0.01)
        self.assertEqual(forwarder.statistics.serial_opens, 1)
        expected = bytes([0xFD, 0x04, 0x00, 0x00, 0x2A, 0x01, 0x01, 0x00, 0x00, 0x00])
        os.write(master, expected)
        actual, _sender = receiver.recvfrom(4096)
        thread.join(timeout=2.0)
        self.assertFalse(thread.is_alive())
        self.assertEqual(actual, expected)
        self.assertEqual(forwarder.statistics.serial_bytes, len(expected))
        self.assertEqual(forwarder.statistics.datagrams, 1)
        os.close(master)
        os.close(slave)
        receiver.close()

    @unittest.skipUnless(pty is not None and tty is not None, "requires POSIX serial")
    def test_missing_serial_is_retried_without_crashing(self) -> None:
        forwarder = MODULE.ReadOnlyForwarder(
            "/dev/efis-test-device-that-does-not-exist",
            [("127.0.0.1", 14550)],
            retry_seconds=0.1,
            report_seconds=60.0,
        )
        self.assertEqual(forwarder.run(seconds=0.25), 0)
        self.assertGreaterEqual(forwarder.statistics.reconnects, 2)
        self.assertEqual(forwarder.statistics.serial_bytes, 0)
        self.assertEqual(forwarder.statistics.datagrams, 0)


if __name__ == "__main__":
    unittest.main()
