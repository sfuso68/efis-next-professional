from __future__ import annotations

from pathlib import Path
import socket
import subprocess
import sys
import unittest


ROOT = Path(__file__).parents[1]
SCRIPT = ROOT / "tools" / "network" / "udp_port_holder.py"


class UdpPortHolderTests(unittest.TestCase):
    def test_holds_then_releases_endpoint_without_transmitting(self) -> None:
        probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        probe.bind(("127.0.0.1", 0))
        port = probe.getsockname()[1]
        probe.close()

        process = subprocess.Popen(
            [
                sys.executable,
                str(SCRIPT),
                "--bind",
                "127.0.0.1",
                "--port",
                str(port),
                "--duration",
                "1",
            ],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        assert process.stdout is not None
        ready = process.stdout.readline()
        self.assertIn('"mode": "RX_HOLD_NO_TRANSMIT"', ready)

        collision = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            with self.assertRaises(OSError):
                collision.bind(("127.0.0.1", port))
        finally:
            collision.close()

        _stdout_tail, stderr = process.communicate(timeout=3)
        self.assertEqual(process.returncode, 0, stderr)
        released = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            released.bind(("127.0.0.1", port))
        finally:
            released.close()


if __name__ == "__main__":
    unittest.main()
