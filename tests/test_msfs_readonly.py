"""Offline protocol/freshness/containment tests; no MSFS or SDK required."""
import ast
import ctypes
import io
import json
import math
from pathlib import Path
import struct
import unittest
import tempfile
from unittest.mock import patch

from tools.simulation import msfs_readonly as m


def packet(kind, payload=b""):
    return struct.pack("<III", 12 + len(payload), 0, kind) + payload


OPEN = packet(2, bytes(288))
VALUES = (45.653165, 12.207559, 55.6, 0.0, -3.0, 0.0, 624.0)


def data(values=VALUES, **changes):
    fields = dict(request=1, obj=0, definition=1, flags=0, entry=1, total=1, count=7)
    fields.update(changes)
    payload = struct.pack("<7I", *fields.values()) + struct.pack("<7d", *values)
    return packet(8, payload)


class FakeClock:
    def __init__(self):
        self.ns = 0

    def now(self):
        return self.ns

    def sleep(self, seconds):
        self.ns += round(seconds * 1e9)


class FakeReader:
    def __init__(self, clock, stop_after=None, quit_after=None, connection_error=False):
        self.clock = clock
        self.stop_after = stop_after
        self.quit_after = quit_after
        self.connection_error = connection_error
        self.closed = False
        self.open_pending = True
        self.last = -100_000_000

    def connect(self):
        if self.connection_error:
            raise OSError("test open failure")

    def close(self):
        self.closed = True

    def poll(self):
        if self.open_pending:
            self.open_pending = False
            return OPEN
        if self.quit_after is not None and self.clock.ns >= self.quit_after:
            return packet(3)
        if self.stop_after is not None and self.clock.ns >= self.stop_after:
            return None
        if self.clock.ns - self.last >= 100_000_000:
            self.last = self.clock.ns
            return data()
        return None


class MsfsReadOnlyTests(unittest.TestCase):
    def test_exact_data_layout_and_sim_metadata(self):
        self.assertEqual(m.decode_packet(data()), ("data", VALUES))
        state = m.Acquisition()
        state.ingest(OPEN, 0, "receipt0")
        state.ingest(data(), 10, "receipt1")
        frame = state.frame(20, "output")
        self.assertEqual(frame["status"], "SIMULATED")
        self.assertTrue(frame["simulated"])
        self.assertFalse(frame["flight_approval"])
        self.assertFalse(frame["aircraft_model_verified"])
        self.assertEqual(frame["received_utc"], "receipt1")
        self.assertEqual(frame["source_sequence"], 1)
        self.assertEqual(len(frame["samples"]), 7)
        self.assertIn("MSFS_NATIVE", frame["axis_convention"])
        for i, (name, var, unit) in enumerate(m.SIGNALS):
            self.assertEqual(frame["samples"][name]["value"], VALUES[i])
            self.assertEqual(frame["samples"][name]["raw"], VALUES[i])
            self.assertEqual(frame["samples"][name]["unit"], unit)
            self.assertEqual(frame["samples"][name]["simvar"], var)

    def test_freshness_boundaries_mask_values_preserve_origin(self):
        state = m.Acquisition()
        state.ingest(OPEN, 0, "utc")
        state.ingest(data(), 0, "utc")
        for ns, expected in [(499_999_999, "SIMULATED"), (500_000_000, "STALE"),
                             (1_999_999_999, "STALE"), (2_000_000_000, "LOST")]:
            frame = state.frame(ns, "utc")
            self.assertEqual(frame["status"], expected)
            if expected != "SIMULATED":
                self.assertTrue(all(x["value"] is None for x in frame["samples"].values()))
                self.assertEqual(frame["samples"]["engine_1_rpm"]["raw"], 624)
            self.assertTrue(frame["simulated"])
        state.ingest(data(), 2_100_000_000, "recovery")
        self.assertEqual(state.frame(2_100_000_001, "utc")["status"], "SIMULATED")
        self.assertEqual(state.source_sequence, 2)

    def test_no_data_times_out_and_output_is_not_source_heartbeat(self):
        state = m.Acquisition()
        self.assertEqual(state.frame(0, "utc")["status"], "WAITING")
        self.assertEqual(state.frame(2_000_000_000, "utc")["status"], "LOST")
        self.assertEqual(state.source_sequence, 0)
        self.assertEqual(state.output_sequence, 2)
        self.assertIsNone(state.frame(2_100_000_000, "utc")["age_ms"])

    def test_clock_and_configuration_fail_closed(self):
        for stale, lost in [(0, 1), (500, 500), (500, math.inf), (math.nan, 2000), (1, 60001)]:
            with self.subTest(stale=stale, lost=lost), self.assertRaises(ValueError):
                m.Acquisition(stale, lost)
        state = m.Acquisition()
        state.frame(100, "utc")
        for now in [99, -1, 1.0, math.nan]:
            with self.subTest(now=now), self.assertRaises(ValueError):
                state.frame(now, "utc")

    def test_nonfinite_and_geographic_values_rejected_atomically(self):
        for index in range(7):
            for bad in [math.nan, math.inf, -math.inf]:
                values = list(VALUES)
                values[index] = bad
                with self.subTest(index=index, bad=bad), self.assertRaises(ValueError):
                    m.decode_packet(data(values))
        for lat, lon in [(91, 12), (45, -181)]:
            with self.assertRaises(ValueError):
                m.decode_packet(data((lat, lon) + VALUES[2:]))

    def test_corrupt_sizes_requests_and_payloads_rejected(self):
        bad = [b"", b"x" * 11, packet(2), packet(1), data()[:-1], data() + b"\0"]
        for field in ["request", "obj", "definition", "flags", "entry", "total", "count"]:
            bad.append(data(**{field: 999}))
        bad.append(bytes(m.MAX_PACKET + 1))
        for blob in bad:
            with self.subTest(size=len(blob)), self.assertRaises(ValueError):
                m.decode_packet(blob)

    def test_exception_quit_and_malformed_are_terminal_and_masked(self):
        for blob, status in [(packet(1, struct.pack("<III", 7, 1, 0)), "FAULT"),
                             (packet(3), "LOST"), (b"invalid", "FAULT")]:
            state = m.Acquisition()
            state.ingest(OPEN, 0, "utc")
            state.ingest(data(), 1, "utc")
            state.ingest(blob, 2, "utc")
            state.ingest(data(), 3, "utc")
            frame = state.frame(4, "utc")
            self.assertEqual(frame["status"], status)
            self.assertEqual(frame["source_sequence"], 1)
            self.assertTrue(all(s["value"] is None for s in frame["samples"].values()))

    def test_data_before_open_rejected(self):
        state = m.Acquisition()
        state.ingest(data(), 0, "utc")
        self.assertEqual(state.frame(1, "utc")["status"], "FAULT")

    def test_unknown_message_ignored_without_refresh(self):
        state = m.Acquisition()
        state.ingest(OPEN, 0, "utc")
        state.ingest(data(), 1, "utc")
        state.ingest(packet(1000), 2_000_000_001, "utc")
        self.assertEqual(state.frame(2_000_000_002, "utc")["status"], "LOST")

    def test_capture_success_and_close(self):
        clock = FakeClock()
        reader = FakeReader(clock)
        output = io.StringIO()
        rc = m.capture(reader, output, duration=1, clock=clock.now, sleep=clock.sleep, utc=lambda: "utc")
        rows = [json.loads(s) for s in output.getvalue().splitlines()]
        self.assertEqual(rc, 0)
        self.assertTrue(reader.closed)
        self.assertEqual(rows[-1]["status"], "PASS")
        self.assertEqual(rows[-1]["source_frames"], 10)
        self.assertEqual(rows[-1]["output_frames"], len(rows)-1)
        self.assertTrue(all(r["simulated"] and not r["flight_approval"] for r in rows))

    def test_capture_stall_quit_and_connect_failure_not_pass(self):
        for kwargs in [dict(stop_after=300_000_000), dict(quit_after=300_000_000),
                       dict(connection_error=True)]:
            clock = FakeClock()
            reader = FakeReader(clock, **kwargs)
            output = io.StringIO()
            with patch("sys.stderr", io.StringIO()):
                rc = m.capture(reader, output, duration=3, clock=clock.now, sleep=clock.sleep, utc=lambda: "utc")
            rows = [json.loads(s) for s in output.getvalue().splitlines()]
            self.assertEqual(rc, 1)
            self.assertTrue(reader.closed)
            self.assertEqual(rows[-1]["status"], "FAIL")
            self.assertEqual(rows[-1]["output_frames"], len(rows)-1)
            self.assertTrue(all(s["value"] is None for s in rows[-2]["samples"].values()))

    def test_capture_invalid_arguments_do_not_connect(self):
        for duration, hz in [(0, 10), (3601, 10), (math.nan, 10), (2, 0), (2, 21), (2, math.inf)]:
            with self.subTest(duration=duration, hz=hz), self.assertRaises(ValueError):
                m.capture(None, io.StringIO(), duration=duration, hz=hz)

    def test_only_allowlisted_sdk_apis_no_publisher(self):
        source = Path(m.__file__).read_text(encoding="utf-8")
        tree = ast.parse(source)
        imports = [n for n in ast.walk(tree) if isinstance(n, (ast.Import, ast.ImportFrom))]
        self.assertNotIn("socket", repr([ast.dump(n) for n in imports]))
        names = {n.value for n in ast.walk(tree) if isinstance(n, ast.Constant)
                 and isinstance(n.value, str) and n.value.startswith("SimConnect_")}
        self.assertEqual(names, {"SimConnect_Open", "SimConnect_Close", "SimConnect_AddToDataDefinition",
                                 "SimConnect_RequestDataOnSimObject", "SimConnect_GetNextDispatch"})
        self.assertIn('0xffffffff), "open"', source)

    def test_platform_and_absolute_dll_path_required(self):
        with patch.object(m.sys, "platform", "linux"), self.assertRaises(OSError):
            m.SimConnectReader(Path("whatever.dll"))
        with patch.object(m.sys, "platform", "win32"), self.assertRaises(OSError):
            m.SimConnectReader(Path("relative.dll"))

    def test_dispatch_errors_not_silently_treated_as_empty(self):
        reader = m.SimConnectReader.__new__(m.SimConnectReader)
        reader.handle = ctypes.c_void_p(1)
        reader.next = lambda *args: -2147467259  # E_FAIL: no message
        self.assertIsNone(reader.poll())
        reader.next = lambda *args: -2147024891  # E_ACCESSDENIED
        with self.assertRaises(OSError):
            reader.poll()
        reader.next = lambda *args: 0  # success but null pointer: malformed
        with self.assertRaises(ValueError):
            reader.poll()

    def test_json_serialization_rejects_nan(self):
        with self.assertRaises(ValueError):
            m.emit(io.StringIO(), {"bad": math.nan})

    def test_output_existing_file_not_overwritten_or_connected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "evidence.jsonl"
            path.write_text("KEEP ORIGINAL", encoding="utf-8")
            with patch.object(m, "SimConnectReader") as reader, patch("sys.stderr", io.StringIO()):
                rc = m.main(["--dll", "D:/sdk/SimConnect.dll", "--output", str(path)])
                reader.return_value.connect.assert_not_called()
            self.assertEqual(rc, 2)
            self.assertEqual(path.read_text(encoding="utf-8"), "KEEP ORIGINAL")

    def test_invalid_cli_arguments_fail_before_loading_dll(self):
        for option, value in [("--hz", "nan"), ("--duration-seconds", "inf"),
                              ("--stale-ms", "0"), ("--lost-ms", "200")]:
            with patch.object(m, "SimConnectReader") as reader, patch("sys.stderr", io.StringIO()):
                try:
                    rc = m.main(["--dll", "D:/sdk/SimConnect.dll", option, value])
                except SystemExit as exc:
                    rc = exc.code
                self.assertEqual(rc, 2)
                reader.assert_not_called()

    def test_ctypes_bindings_are_fixed_width_local_only_and_read_only(self):
        class Function:
            def __init__(self, name):
                self.name = name
                self.calls = []

            def __call__(self, *args):
                self.calls.append(args)
                if self.name == "SimConnect_Open":
                    ctypes.cast(args[0], ctypes.POINTER(ctypes.c_void_p))[0] = 123
                return 0

        class DLL:
            def __init__(self):
                self.functions = {}

            def __getattr__(self, name):
                self.functions.setdefault(name, Function(name))
                return self.functions[name]

        dll = DLL()
        # Simulate Windows loading only; no SDK is executed by this test.
        with patch.object(m.sys, "platform", "win32"), patch.object(Path, "is_file", return_value=True), \
             patch.object(ctypes, "WinDLL", return_value=dll, create=True):
            reader = m.SimConnectReader(Path(__file__).resolve())
            reader.connect()
            reader.close()
            reader.close()
        self.assertEqual(dll.functions["SimConnect_Open"].calls[0][-1], 0xffffffff)
        self.assertEqual(len(dll.functions["SimConnect_AddToDataDefinition"].calls), 7)
        self.assertEqual(dll.functions["SimConnect_RequestDataOnSimObject"].calls[0][1:],
                         (1, 1, 0, 3, 0, 0, 0, 0))
        self.assertEqual(len(dll.functions["SimConnect_Close"].calls), 1)
        for fn in dll.functions.values():
            self.assertEqual(ctypes.sizeof(fn.restype), 4)
        self.assertEqual(ctypes.sizeof(dll.functions["SimConnect_Open"].argtypes[-1]), 4)


if __name__ == "__main__":
    unittest.main()
