"""Bounded, single-owner software SIL session; no network or actuator transport.

Backend and journal are trusted injected objects. Consumer callbacks never run
in the producer. This candidate is not yet the live JSBSim/R1-20 adapter.
"""
import copy
import hashlib
import json
import os
from pathlib import Path
import threading
import time

from tools.simulation import session_contract as contract

TICK_NS = 10_000_000
MAX_LAG_NS = 100_000_000  # Existing FDM lateness limit, not a consumer freshness budget.


def encode(value):
    return (json.dumps(value, sort_keys=True, separators=(",", ":"), allow_nan=False)+"\n").encode()


class TruthJournal:
    """Exclusive new run directory; durable each append, no retention/overwrite.

    Slow storage counts against the next producer deadline. Storage failure
    stops the run; this is a test journal, never the operational recorder.
    """
    def __init__(self, directory):
        directory = Path(directory)
        directory.mkdir(parents=False, exist_ok=False)
        self.stream = (directory / "truth.jsonl").open("xb", buffering=0)
        self.digest = hashlib.sha256()
        self.records = 0
        self.closed = False

    def append(self, record):
        data = encode(record)
        if self.closed:
            raise ValueError("journal closed")
        written = self.stream.write(data)
        if written != len(data):
            raise OSError("short journal write")
        os.fsync(self.stream.fileno())
        self.digest.update(data)
        self.records += 1

    def close(self):
        if not self.closed:
            self.closed = True
            self.stream.close()

    def finish(self, report):
        self.close()
        result = dict(report, truth_sha256=self.digest.hexdigest(), truth_records=self.records)
        path = Path(self.stream.name).parent / "result.json"
        pending = path.with_name("result.pending.json")
        with pending.open("xb", buffering=0) as output:
            data = encode(result)
            if output.write(data) != len(data):
                raise OSError("short result write")
            os.fsync(output.fileno())
        # Exclusive atomic publication, never overwrite a prior result. The
        # pending candidate remains evidence; only result.json is completion.
        if os.name == "nt":
            # Windows rename is atomic and refuses an existing destination.
            os.rename(pending, path)
        else:
            # POSIX rename would replace an existing result; use an exclusive
            # hardlink instead. Unsupported filesystems fail closed.
            os.link(pending, path)


class LatestSlot:
    """One immutable packet; producer never waits on a consumer-held lock."""
    def __init__(self):
        self.lock = threading.Lock()
        self.packet = None
        self.overwritten = 0
        self.busy_drops = 0

    def publish(self, epoch, packet):
        if not self.lock.acquire(blocking=False):
            self.busy_drops += 1
            return
        try:
            if self.packet is not None:
                self.overwritten += 1
            self.packet = (epoch, packet)
        finally:
            self.lock.release()

    def take(self, epoch):
        with self.lock:
            packet, self.packet = self.packet, None
        return packet[1] if packet is not None and packet[0] == epoch else None


class SessionEngine:
    """Owner-thread lifecycle/pump; only poll/status access is for consumers.

    The backend implements step() and frame(manifest, relative_tick, mono_ns).
    It must preserve physical state across pause; relative clocks reset on
    resume while total_ticks remains in the truth log. No seek is implemented.
    """
    def __init__(self, manifest, backend, journal, *, clock=time.monotonic_ns, max_ticks=6000):
        contract.manifest(manifest)
        contract.integer(max_ticks, 1, 360000)
        if manifest["mode"] != "LIVE":
            raise ValueError("this scheduler requires LIVE 1x mode")
        self._manifest = copy.deepcopy(manifest)
        self.backend, self.journal, self.clock = backend, journal, clock
        self.max_ticks = max_ticks
        self.owner = threading.get_ident()
        self.state = "CREATED"
        self.total_ticks = self.relative_tick = self.frames = 0
        self.slots = {}
        self.failure = None
        self.last_clock = None
        self.epoch_start = self.epoch_total_start = self.next_deadline = None
        self.maximum_lag_ns = 0
        self._event("CREATED")

    @property
    def manifest(self):
        return copy.deepcopy(self._manifest)

    def _owner(self):
        if threading.get_ident() != self.owner:
            raise RuntimeError("producer/lifecycle requires owner thread")

    def _now(self):
        now = self.clock()
        contract.integer(now)
        if self.last_clock is not None and now < self.last_clock:
            raise ValueError("monotonic clock reversed")
        self.last_clock = now
        return now

    def _event(self, event, **extra):
        try:
            self.journal.append(dict(kind="SESSION_EVENT", event=event,
                                     session_id=self._manifest["session_id"],
                                     epoch=self._manifest["epoch"], total_ticks=self.total_ticks,
                                     simulated=True, flight_approval=False, **extra))
        except Exception:
            self.state, self.failure = "FAILED", "JOURNAL_FAILURE"
            self.journal.close()
            raise

    def _fail(self, reason):
        self.state, self.failure = "FAILED", reason
        try:
            self._event("FAILED", reason=reason)
        finally:
            self.journal.close()

    def register(self, name):
        self._owner()
        contract.token(name)
        if self.state != "CREATED" or name in self.slots or len(self.slots) >= 8:
            raise ValueError("consumer registration boundary")
        self.slots[name] = LatestSlot()

    def validate(self):
        self._owner()
        if self.state != "CREATED":
            raise ValueError("validate transition")
        self._event("VALIDATED", manifest=self._manifest)
        self.state = "VALIDATED"

    def arm_software_test(self):
        self._owner()
        if self.state != "VALIDATED":
            raise ValueError("arm transition")
        self._event("ARMED_TEST", scope="SOFTWARE_ONLY_NO_ENDPOINTS")
        self.state = "ARMED_TEST"

    def _emit(self, now):
        value = self.backend.frame(copy.deepcopy(self._manifest), self.relative_tick, now)
        contract.frame(value, self._manifest)
        if value["tick"] != self.relative_tick or value["producer_mono_ns"] != now:
            raise ValueError("backend frame clock mismatch")
        packet = encode(value)
        if len(packet) > 65536:
            raise ValueError("oversize frame")
        self._event("FRAME", total_frames=self.frames, frame=value)
        if self._now() - now > MAX_LAG_NS:
            raise ValueError("frame persistence deadline exceeded")
        # Sync mode persists; explicit async-test mode only queues. The latter
        # has a frozen policy/watchdog and must not claim immediate durability.
        for slot in self.slots.values():
            slot.publish(self._manifest["epoch"], packet)
        self.frames += 1

    def _begin_epoch(self):
        now = self._now()
        self.epoch_start = now
        self.epoch_total_start = self.total_ticks
        self.relative_tick = 0
        self.next_deadline = now + TICK_NS
        self._event("RUNNING", producer_mono_ns=now)
        self.state = "RUNNING"
        self._emit(now)

    def start(self):
        self._owner()
        if self.state != "ARMED_TEST":
            raise ValueError("start transition")
        try:
            self._begin_epoch()
        except Exception as exc:
            if self.state != "FAILED":
                self._fail(type(exc).__name__)
            raise

    def pump(self):
        self._owner()
        if self.state != "RUNNING":
            return False
        try:
            check = getattr(self.journal, "check", None)
            if check is not None: check()
            now = self._now()
            if now < self.next_deadline:
                return False
            expected = self.epoch_start + (self.total_ticks-self.epoch_total_start+1)*TICK_NS
            lag = now - expected
            self.maximum_lag_ns = max(self.maximum_lag_ns, lag)
            if lag > MAX_LAG_NS:
                raise ValueError("producer deadline exceeded")
            # One physical step per call, no burst/catch-up, never skip an FDM tick.
            self.backend.step()
            self.total_ticks += 1
            self.relative_tick += 1
            after_step = self._now()
            if after_step - expected > MAX_LAG_NS:
                raise ValueError("backend deadline exceeded")
            # Preserve the deadline phase for ordinary sleep jitter; repeatedly
            # rebasing to actual wake-up would accumulate drift on every tick.
            nominal_next = self.next_deadline + TICK_NS
            self.next_deadline = nominal_next if nominal_next > after_step else after_step + TICK_NS
            if self.relative_tick % 5 == 0:
                self._emit(now)
            if self.total_ticks >= self.max_ticks:
                self.stop()
            return True
        except Exception as exc:
            if self.state != "FAILED":
                self._fail(type(exc).__name__)
            raise

    def pause(self):
        self._owner()
        if self.state != "RUNNING":
            raise ValueError("pause transition")
        self._event("PAUSED")
        self.state = "PAUSED"

    def resume(self):
        self._owner()
        if self.state != "PAUSED":
            raise ValueError("resume transition")
        try:
            self._manifest["epoch"] += 1
            contract.manifest(self._manifest)
            self._event("EPOCH", parent_total_ticks=self.total_ticks)
            self._begin_epoch()
        except Exception as exc:
            if self.state != "FAILED":
                self._fail(type(exc).__name__)
            raise

    def stop(self):
        self._owner()
        if self.state not in ("RUNNING", "PAUSED", "ARMED_TEST"):
            raise ValueError("stop transition")
        self.state = "STOPPING"
        try:
            self._event("STOPPING")
            self._event("END_OF_DATA", frames=self.frames, maximum_lag_ns=self.maximum_lag_ns)
            self.journal.finish(dict(schema="fuso-avionics.sil-session-result.v1",
                status="COMPLETE_SOFTWARE_ONLY", session_id=self._manifest["session_id"],
                simulated=True, flight_approval=False, frames=self.frames,
                total_ticks=self.total_ticks, maximum_lag_ns=self.maximum_lag_ns,
                hardware_acceptance="NOT_IMPLIED"))
            self.state = "COMPLETE"
        except Exception:
            self.state, self.failure = "FAILED", "JOURNAL_FINALIZATION_FAILURE"
            self.journal.close()
            raise

    def poll(self, consumer):
        if self.state != "RUNNING":
            return None
        return self.slots[consumer].take(self._manifest["epoch"])

    def run(self, *, sleep=time.sleep):
        """Owner loop; no requests/acknowledgements from a display are needed."""
        self._owner()
        while self.state == "RUNNING":
            if not self.pump():
                sleep(max(0, self.next_deadline-self._now()) / 1e9)
