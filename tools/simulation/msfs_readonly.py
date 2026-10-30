"""Local MSFS SimConnect acquisition. No network publisher or simulation setters.

Development JSONL, NOT a DDP/EDP transport. Only the seven bench-proven SimVars
are requested. Host receipt timestamps are not aircraft measurement timestamps.
"""
from __future__ import annotations

import argparse
import ctypes
from datetime import datetime, timezone
import json
import math
from pathlib import Path
import struct
import sys
import time
import uuid

SCHEMA = "fuso-avionics.msfs-readonly.v1"
# Name, SDK SimVar, requested unit. Pitch sign is deliberately not converted.
SIGNALS = (
    ("latitude_deg", "PLANE LATITUDE", "degrees"),
    ("longitude_deg", "PLANE LONGITUDE", "degrees"),
    ("altitude_msl_ft", "PLANE ALTITUDE", "feet"),
    ("ias_kt", "AIRSPEED INDICATED", "knots"),
    ("pitch_msfs_deg", "PLANE PITCH DEGREES", "degrees"),
    ("bank_msfs_deg", "PLANE BANK DEGREES", "degrees"),
    ("engine_1_rpm", "GENERAL ENG RPM:1", "rpm"),
)
HEADER = struct.Struct("<III")
DATA_HEADER = struct.Struct("<10I")
PAYLOAD = struct.Struct("<7d")
MAX_PACKET = 65536


def decode_packet(packet: bytes):
    """Decode only fixed layouts requested here, rejecting truncated/foreign data."""
    if not HEADER.size <= len(packet) <= MAX_PACKET:
        raise ValueError("packet size")
    size, _version, kind = HEADER.unpack_from(packet)
    if size != len(packet):
        raise ValueError("declared packet size")
    if kind == 2:
        # SDK SIMCONNECT_RECV_OPEN: base, application[256], eight DWORDs.
        if size < 300:
            raise ValueError("truncated open")
        return "open", None
    if kind == 3:
        return "quit", None
    if kind == 1:
        if size < 24:
            raise ValueError("truncated exception")
        return "exception", struct.unpack_from("<III", packet, 12)
    if kind != 8:
        return "ignored", None
    if size != DATA_HEADER.size + PAYLOAD.size:
        raise ValueError("data layout")
    _, _, _, request, obj, definition, flags, entry, total, count = DATA_HEADER.unpack_from(packet)
    if (request, obj, definition, flags, count) != (1, 0, 1, 0, 7):
        raise ValueError("foreign request/object/definition/flags/count")
    if entry != 1 or total != 1:
        raise ValueError("unexpected multipart data")
    values = PAYLOAD.unpack_from(packet, DATA_HEADER.size)
    if not all(math.isfinite(v) for v in values):
        raise ValueError("nonfinite sample")
    if abs(values[0]) > 90 or abs(values[1]) > 180:
        raise ValueError("geographic range")
    return "data", values


class Acquisition:
    def __init__(self, stale_ms=500, lost_ms=2000):
        if not (math.isfinite(stale_ms) and math.isfinite(lost_ms)
                and 0 < stale_ms < lost_ms <= 60000):
            raise ValueError("freshness limits")
        self.stale_ms, self.lost_ms = stale_ms, lost_ms
        self.instance = str(uuid.uuid4())
        self.opened = False
        self.terminal = None
        self.reason = None
        self.values = None
        self.received_ns = None
        self.received_utc = None
        self.source_sequence = 0
        self.output_sequence = 0
        self.last_ns = None
        self.first_ns = None

    def _clock(self, now_ns):
        if not isinstance(now_ns, int) or now_ns < 0:
            raise ValueError("invalid monotonic timestamp")
        if self.last_ns is not None and now_ns < self.last_ns:
            raise ValueError("monotonic clock moved backwards")
        self.last_ns = now_ns
        if self.first_ns is None:
            self.first_ns = now_ns

    def ingest(self, packet, now_ns, utc):
        self._clock(now_ns)
        if self.terminal:
            return
        try:
            kind, payload = decode_packet(packet)
        except ValueError:
            self.fail("FAULT", "MALFORMED_PACKET")
            return
        if kind == "open":
            self.opened = True
        elif kind == "exception":
            self.fail("FAULT", f"SIMCONNECT_EXCEPTION_{payload[0]}")
        elif kind == "quit":
            self.fail("LOST", "SIMULATOR_QUIT")
        elif kind == "data":
            if not self.opened:
                self.fail("FAULT", "DATA_BEFORE_OPEN")
                return
            self.values = payload
            self.received_ns, self.received_utc = now_ns, utc
            self.source_sequence += 1

    def fail(self, status, reason):
        if status not in ("FAULT", "LOST"):
            raise ValueError("terminal state")
        self.terminal, self.reason = status, reason

    def frame(self, now_ns, utc):
        self._clock(now_ns)
        age_ms = None if self.received_ns is None else (now_ns - self.received_ns) / 1e6
        if self.terminal:
            status = self.terminal
        elif age_ms is None:
            status = "LOST" if (now_ns - self.first_ns) / 1e6 >= self.lost_ms else "WAITING"
        elif age_ms >= self.lost_ms:
            status = "LOST"
        elif age_ms >= self.stale_ms:
            status = "STALE"
        else:
            status = "SIMULATED"
        self.output_sequence += 1
        usable = status == "SIMULATED"
        return {
            "schema": SCHEMA, "type": "frame", "source": "MSFS2024_SIMCONNECT",
            "simulated": True, "flight_approval": False, "instance": self.instance,
            "output_sequence": self.output_sequence,
            "source_sequence": self.source_sequence,
            "emitted_monotonic_ns": now_ns, "emitted_utc": utc,
            "received_monotonic_ns": self.received_ns,
            "received_utc": self.received_utc, "timestamp_basis": "HOST_RECEIPT_NOT_SIM_TIME",
            "age_ms": age_ms, "status": status, "reason": self.reason,
            "stale_after_ms": self.stale_ms, "lost_after_ms": self.lost_ms,
            "aircraft_model_verified": False,
            "axis_convention": "MSFS_NATIVE_NOT_YET_MAPPED_TO_EFIS",
            "samples": {
                name: {"value": self.values[i] if usable else None,
                       "raw": self.values[i] if self.values is not None else None,
                       "unit": unit, "simvar": var, "validity": status, "simulated": True}
                for i, (name, var, unit) in enumerate(SIGNALS)
            },
        }


class SimConnectReader:
    """Exactly five SDK functions, local-only connection; no setter/event API."""
    def __init__(self, dll_path):
        if sys.platform != "win32" or ctypes.sizeof(ctypes.c_void_p) != 8:
            raise OSError("Windows x64 Python required")
        path = Path(dll_path)
        if not path.is_absolute() or not path.is_file():
            raise OSError("absolute installed SDK DLL path required")
        self.dll = ctypes.WinDLL(str(path.resolve()), winmode=0x100 | 0x800)
        u32, ptr, hr = ctypes.c_uint32, ctypes.c_void_p, ctypes.c_int32
        def bind(name, args):
            func = getattr(self.dll, name)
            func.argtypes, func.restype = args, hr
            return func
        self.open = bind("SimConnect_Open", [ctypes.POINTER(ptr), ctypes.c_char_p,
                         ptr, u32, ptr, u32])
        self.close_fn = bind("SimConnect_Close", [ptr])
        self.define = bind("SimConnect_AddToDataDefinition",
                           [ptr, u32, ctypes.c_char_p, ctypes.c_char_p, u32, ctypes.c_float, u32])
        self.request = bind("SimConnect_RequestDataOnSimObject", [ptr] + [u32] * 8)
        self.next = bind("SimConnect_GetNextDispatch", [ptr, ctypes.POINTER(ptr), ctypes.POINTER(u32)])
        self.handle = ptr()

    @staticmethod
    def check(result, operation):
        if result < 0:
            raise OSError(f"{operation}: HRESULT 0x{result & 0xffffffff:08x}")

    def connect(self):
        self.check(self.open(ctypes.byref(self.handle), b"FUSO AVIONICS READ ONLY",
                            None, 0, None, 0xffffffff), "open")
        try:
            for _, var, unit in SIGNALS:
                self.check(self.define(self.handle, 1, var.encode("ascii"), unit.encode("ascii"),
                                       4, 0.0, 0xffffffff), "define")
            # SDK PERIOD_SIM_FRAME=3. Output clock does not pretend to be source clock.
            self.check(self.request(self.handle, 1, 1, 0, 3, 0, 0, 0, 0), "request")
        except Exception:
            self.close()
            raise

    def poll(self):
        packet, size = ctypes.c_void_p(), ctypes.c_uint32()
        result = self.next(self.handle, ctypes.byref(packet), ctypes.byref(size))
        if result & 0xffffffff == 0x80004005:  # SDK E_FAIL also means an empty queue.
            return None
        self.check(result, "dispatch")
        if not packet.value or not HEADER.size <= size.value <= MAX_PACKET:
            raise ValueError("invalid dispatch pointer/size")
        return ctypes.string_at(packet, size.value)

    def close(self):
        if self.handle.value:
            self.close_fn(self.handle)
            self.handle.value = None


def utc_now():
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds")


def emit(stream, record):
    stream.write(json.dumps(record, allow_nan=False, separators=(",", ":")) + "\n")
    stream.flush()


def capture(reader, stream, *, duration=30, hz=10, stale_ms=500, lost_ms=2000,
            clock=time.monotonic_ns, utc=utc_now, sleep=time.sleep):
    if not (math.isfinite(duration) and 1 <= duration <= 3600
            and math.isfinite(hz) and 1 <= hz <= 20):
        raise ValueError("duration/hz bounds")
    state = Acquisition(stale_ms, lost_ms)
    emitted = stale_frames = 0
    failures = 0
    ever_lost = False
    start = clock()
    next_output = start
    period = round(1e9 / hz)
    try:
        reader.connect()
        while clock() - start < round(duration * 1e9):
            for _ in range(256):  # bounded draining; never starve timeout processing
                packet = reader.poll()
                if packet is None:
                    break
                state.ingest(packet, clock(), utc())
                if state.terminal:
                    break
            now = clock()
            if now >= next_output or state.terminal:
                frame = state.frame(now, utc())
                emit(stream, frame)
                emitted += 1
                stale_frames += frame["status"] == "STALE"
                ever_lost |= frame["status"] == "LOST"
                next_output = now + period
            if state.terminal:
                break
            sleep(0.005)
    except (OSError, ValueError) as exc:
        state.fail("FAULT", "ACQUISITION_ERROR")
        emit(stream, state.frame(clock(), utc()))
        emitted += 1
        print(str(exc), file=sys.stderr)
        failures += 1
    except KeyboardInterrupt:
        state.fail("LOST", "USER_STOP")
        emit(stream, state.frame(clock(), utc()))
        emitted += 1
    finally:
        reader.close()
    received = state.source_sequence
    status = "PASS" if received >= 3 and not state.terminal and not ever_lost and not failures and not stale_frames else "FAIL"
    summary = {"schema": SCHEMA, "type": "summary", "status": status,
               "simulated": True, "flight_approval": False,
               "source_frames": received, "output_frames": emitted,
               "stale_output_frames": stale_frames,
               "ever_lost": ever_lost, "terminal_reason": state.reason,
               "scope": "LOCAL_ACQUISITION_ONLY_NOT_EFIS_ACCEPTANCE"}
    emit(stream, summary)
    return 0 if status == "PASS" else 1


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dll", required=True, type=Path, help="absolute installed SDK SimConnect.dll")
    parser.add_argument("--duration-seconds", type=float, default=30)
    parser.add_argument("--hz", type=float, default=10)
    parser.add_argument("--stale-ms", type=float, default=500)
    parser.add_argument("--lost-ms", type=float, default=2000)
    parser.add_argument("--output", type=Path, help="new JSONL file; existing files NEVER overwritten")
    args = parser.parse_args(argv)
    if not (math.isfinite(args.duration_seconds) and 1 <= args.duration_seconds <= 3600
            and math.isfinite(args.hz) and 1 <= args.hz <= 20):
        parser.error("duration must be 1..3600 seconds, hz 1..20, all finite")
    try:
        Acquisition(args.stale_ms, args.lost_ms)
        reader = SimConnectReader(args.dll)
        if args.output:
            with args.output.open("x", encoding="utf-8", newline="\n") as stream:
                return capture(reader, stream, duration=args.duration_seconds, hz=args.hz,
                               stale_ms=args.stale_ms, lost_ms=args.lost_ms)
        return capture(reader, sys.stdout, duration=args.duration_seconds, hz=args.hz,
                       stale_ms=args.stale_ms, lost_ms=args.lost_ms)
    except (OSError, ValueError) as exc:
        print(f"read-only bridge: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
