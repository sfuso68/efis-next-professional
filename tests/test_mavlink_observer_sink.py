from __future__ import annotations

import importlib.util
import errno
import json
from pathlib import Path
import sys
import tempfile
import unittest


ROOT = Path(__file__).parents[1]
SCRIPT = ROOT / "tools" / "network" / "mavlink_observer_sink.py"
SPEC = importlib.util.spec_from_file_location("mavlink_observer_sink", SCRIPT)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


def frame(*, message_id: int = 0, payload: bytes = b"", signed: bool = False) -> bytes:
    incompatibility_flags = 1 if signed else 0
    header = bytes((
        0xFD,
        len(payload),
        incompatibility_flags,
        0,
        17,
        1,
        1,
    )) + message_id.to_bytes(3, "little")
    signature = bytes(13) if signed else b""
    return header + payload + bytes(2) + signature


class MavlinkObserverSinkTests(unittest.TestCase):
    def test_parses_multiple_unsigned_frames(self) -> None:
        parsed = MODULE.parse_mavlink2_datagram(
            frame(message_id=0) + frame(message_id=33, payload=b"abc")
        )
        self.assertEqual(parsed, [(1, 1, 0, 17), (1, 1, 33, 17)])

    def test_parses_signed_frame(self) -> None:
        self.assertEqual(
            MODULE.parse_mavlink2_datagram(frame(message_id=24, signed=True))[0][2],
            24,
        )

    def test_rejects_v1_and_truncation(self) -> None:
        with self.assertRaises(MODULE.FrameError):
            MODULE.parse_mavlink2_datagram(b"\xfe" + bytes(20))
        with self.assertRaises(MODULE.FrameError):
            MODULE.parse_mavlink2_datagram(frame(payload=b"data")[:-1])

    def test_rejects_unknown_incompatibility_flag(self) -> None:
        invalid = bytearray(frame())
        invalid[2] = 0x02
        with self.assertRaises(MODULE.FrameError):
            MODULE.parse_mavlink2_datagram(bytes(invalid))

    def test_health_contains_counts_not_payload(self) -> None:
        health = MODULE.ObserverHealth(
            bind="192.168.1.6:14550",
            allowed_source="192.168.1.47",
        )
        payload = frame(message_id=0, payload=b"private-flight-data")
        health.accept(payload, MODULE.parse_mavlink2_datagram(payload))
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "health.json"
            MODULE.write_health(path, health)
            raw = path.read_text(encoding="utf-8")
            document = json.loads(raw)
        self.assertEqual(document["mode"], "RX_ONLY_NO_RETURN_PATH")
        self.assertEqual(document["datagrams"], 1)
        self.assertEqual(document["heartbeats"], 1)
        self.assertNotIn("private-flight-data", raw)

    def test_source_rejection_is_accounted_separately(self) -> None:
        health = MODULE.ObserverHealth()
        health.reject_source()
        health.reject_frame()
        self.assertEqual(health.source_rejected, 1)
        self.assertEqual(health.malformed, 1)
        self.assertEqual(health.datagrams, 0)

    def test_bind_wait_retries_only_address_not_available(self) -> None:
        class FakeSocket:
            def __init__(self, bind_error: OSError | None = None) -> None:
                self.bind_error = bind_error
                self.closed = False

            def bind(self, _endpoint: tuple[str, int]) -> None:
                if self.bind_error:
                    raise self.bind_error

            def close(self) -> None:
                self.closed = True

        first = FakeSocket(OSError(errno.EADDRNOTAVAIL, "not assigned"))
        second = FakeSocket()
        sockets = iter((first, second))
        ticks = iter((10.0, 10.1))
        sleeps: list[float] = []

        receiver, retries = MODULE.open_receiver(
            "192.168.1.6",
            14550,
            60.0,
            socket_factory=lambda *_args: next(sockets),
            monotonic=lambda: next(ticks),
            sleeper=sleeps.append,
        )

        self.assertIs(receiver, second)
        self.assertTrue(first.closed)
        self.assertEqual(retries, 1)
        self.assertEqual(sleeps, [0.25])

    def test_bind_wait_does_not_hide_other_socket_errors(self) -> None:
        class DeniedSocket:
            def bind(self, _endpoint: tuple[str, int]) -> None:
                raise OSError(errno.EACCES, "denied")

            def close(self) -> None:
                pass

        with self.assertRaises(OSError) as raised:
            MODULE.open_receiver(
                "192.168.1.6",
                14550,
                60.0,
                socket_factory=lambda *_args: DeniedSocket(),
                monotonic=lambda: 10.0,
                sleeper=lambda _seconds: None,
            )
        self.assertEqual(raised.exception.errno, errno.EACCES)


if __name__ == "__main__":
    unittest.main()
