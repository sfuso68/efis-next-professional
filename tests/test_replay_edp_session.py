from __future__ import annotations

import contextlib
import importlib.util
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).parents[1]
NETWORK = ROOT / "tools" / "network"
sys.path.insert(0, str(NETWORK))
SCRIPT = NETWORK / "replay_edp_session.py"
SPEC = importlib.util.spec_from_file_location("replay_edp_session", SCRIPT)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class FakeSocket:
    def __init__(self) -> None:
        self.sent: list[tuple[bytes, tuple[str, int]]] = []
        self.closed = False

    def sendto(self, payload: bytes, endpoint: tuple[str, int]) -> int:
        self.sent.append((payload, endpoint))
        return len(payload)

    def close(self) -> None:
        self.closed = True


class ReplayEdpSessionTests(unittest.TestCase):
    def frames(self, scenario: str, count: int = 8):
        return MODULE.scenario_frames(scenario, count, 47, 1234, 1.25)

    def test_encoder_matches_independent_parser_for_provenance(self) -> None:
        for simulated in (0, 9, 18):
            with self.subTest(simulated=simulated):
                frame = MODULE.Frame(1, 1234, 47, simulated_samples=simulated)
                decoded = MODULE.edp.parse_edp_v1(MODULE.encode_frame(frame, 1_000_000))
                self.assertEqual(decoded.simulated_samples, simulated)
                self.assertEqual((decoded.source_node, decoded.publisher_instance,
                                  decoded.sequence), (47, 1234, 1))

    def test_scenarios_have_expected_wire_effects(self) -> None:
        for name in MODULE.SCENARIOS - {"bad-crc"}:
            with self.subTest(name=name):
                frames = self.frames(name)
                decoded = [MODULE.edp.parse_edp_v1(
                    MODULE.encode_frame(frame, 1_000_000 + index))
                    for index, frame in enumerate(frames)]
                self.assertEqual(len(decoded), 8)
        corrupt = self.frames("bad-crc")[4]
        with self.assertRaisesRegex(MODULE.edp.EdpError, "CRC_MISMATCH"):
            MODULE.edp.parse_edp_v1(MODULE.encode_frame(corrupt, 1_000_000))

    def test_sequence_faults_are_observable(self) -> None:
        expectations = {
            "gap": (1, 0, 0, 0), "duplicate": (0, 1, 0, 0),
            "reversed": (0, 0, 1, 0), "restart": (0, 0, 0, 1),
        }
        for name, expected in expectations.items():
            streams = {}; restarts = 0
            for index, frame in enumerate(self.frames(name)):
                envelope = MODULE.edp.parse_edp_v1(MODULE.encode_frame(frame, 1_000_000 + index))
                restarts += MODULE.edp.observe_sequence(streams, envelope)
            totals = (sum(stream.gaps for stream in streams.values()),
                      sum(stream.duplicates for stream in streams.values()),
                      sum(stream.reversed for stream in streams.values()), restarts)
            with self.subTest(name=name):
                self.assertEqual(totals, expected)

    def test_authority_and_provenance_scenarios_are_explicit(self) -> None:
        wrong = self.frames("wrong-source")
        self.assertEqual(wrong[4].source_node, 48)
        mixed = self.frames("mixed-provenance")
        self.assertEqual(mixed[4].simulated_samples, 9)
        self.assertTrue(all(frame.simulated_samples == 0 for frame in self.frames("real-only")))
        loss = self.frames("loss-recovery")
        self.assertEqual(sum(frame.pause_before_seconds > 1.0 for frame in loss), 1)

    def test_target_policy_is_fail_closed(self) -> None:
        self.assertEqual(MODULE.validate_target("127.0.0.1", send=True,
                                               allow_bench_network=False), "127.0.0.1")
        with self.assertRaises(MODULE.ReplayError):
            MODULE.validate_target("10.77.10.12", send=True, allow_bench_network=False)
        self.assertEqual(MODULE.validate_target("10.77.10.12", send=True,
                                               allow_bench_network=True), "10.77.10.12")
        for address in ("0.0.0.0", "224.0.0.1", "8.8.8.8",
                        "10.77.10.0", "10.77.10.255", "169.254.1.2"):
            with self.subTest(address=address), self.assertRaises(MODULE.ReplayError):
                MODULE.validate_target(address, send=True, allow_bench_network=True)

    def test_bounds_reject_unsafe_or_invalid_configuration(self) -> None:
        with self.assertRaises(MODULE.ReplayError):
            self.frames("unknown")
        for count in (3, MODULE.MAX_FRAMES + 1):
            with self.subTest(count=count), self.assertRaises(MODULE.ReplayError):
                MODULE.scenario_frames("clean", count, 47, 1234, 1.25)
        for rate in (0.0, MODULE.MAX_RATE_HZ + 0.1, float("nan")):
            with self.subTest(rate=rate), self.assertRaises(MODULE.ReplayError):
                MODULE.checked_float(rate, "rate_hz", 0.1, MODULE.MAX_RATE_HZ)

    def test_send_uses_udp_closes_socket_and_honors_pause(self) -> None:
        fake = FakeSocket(); sleeps: list[float] = []
        frames = self.frames("loss-recovery", count=4)
        sent = MODULE.send_frames(frames, "127.0.0.1", 49003, 20.0,
                                  socket_factory=lambda *_: fake,
                                  sleep=sleeps.append)
        self.assertEqual((sent, len(fake.sent), fake.closed), (4, 4, True))
        self.assertIn(1.25, sleeps)
        self.assertTrue(all(endpoint == ("127.0.0.1", 49003)
                            for _, endpoint in fake.sent))

    def test_manifest_is_reproducible_and_contains_no_payload(self) -> None:
        frames = self.frames("clean")
        first = MODULE.build_manifest(scenario="clean", frames=frames, rate_hz=20.0,
                                      target="127.0.0.1", port=49003, sent=0,
                                      transmission="DRY_RUN")
        second = MODULE.build_manifest(scenario="clean", frames=frames, rate_hz=20.0,
                                       target="127.0.0.1", port=49003, sent=0,
                                       transmission="DRY_RUN")
        self.assertEqual(first["wire_sha256"], second["wire_sha256"])
        loss = MODULE.build_manifest(
            scenario="loss-recovery", frames=self.frames("loss-recovery"),
            rate_hz=20.0, target="127.0.0.1", port=49003, sent=0,
            transmission="DRY_RUN")
        self.assertNotEqual(first["schedule_sha256"], loss["schedule_sha256"])
        self.assertEqual(loss["maximum_pause_seconds"], 1.25)
        self.assertGreater(loss["scheduled_duration_seconds"],
                           first["scheduled_duration_seconds"])
        self.assertFalse(first["raw_payload_persisted"])
        self.assertFalse(first["flight_approval"])
        encoded = json.dumps(first)
        self.assertNotIn("base64", encoded.lower())
        self.assertNotIn("payload", encoded.lower().replace("raw_payload_persisted", ""))

    def test_cli_defaults_to_dry_run_and_writes_atomic_manifest(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "replay.json"
            with mock.patch.object(sys, "argv", [str(SCRIPT), "--frames", "4",
                                                  "--output", str(output)]):
                with mock.patch.object(MODULE, "send_frames") as sender:
                    with contextlib.redirect_stdout(io.StringIO()):
                        self.assertEqual(MODULE.main(), 0)
            sender.assert_not_called()
            manifest = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(manifest["transmission"], "DRY_RUN")
            self.assertEqual(manifest["frames_sent"], 0)


if __name__ == "__main__":
    unittest.main()
