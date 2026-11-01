import threading
import unittest
from unittest.mock import patch

from tools.simulation import session_contract as c
from tools.simulation.local_console import LocalConsole
from tools.simulation.session_engine import encode, TICK_NS
from tests import test_control_execution as execution_fixture


class LocalConsoleTests(unittest.TestCase):
    def setUp(self):
        # Composition fixture only; do not inherit and rerun executor tests.
        self.fixture = execution_fixture.ControlExecutionTests()
        self.fixture.setUp()
        self.addCleanup(self.fixture.doCleanups)
        self.engine, self.clock, self.fdm = self.fixture.engine, self.fixture.clock, self.fixture.fdm
        self.console = LocalConsole(self.engine, self.fixture.control)

    def send(self, obj):
        self.assertEqual(self.console.offer(encode(obj)), "QUEUED_NOT_VALIDATED")
        self.console.pump()
        replies = c.decode(self.console.latest())
        return replies[0] if isinstance(replies, list) else replies

    def command(self, **changes):
        obj = dict(action="SET_CONTROL", command_id="input-1", sequence=1, control="aileron", value=.2)
        obj.update(changes)
        return obj

    def step(self):
        self.clock.now += TICK_NS
        self.console.pump()

    def test_control_readback_and_exact_retry_do_not_reapply_or_restamp(self):
        obj = self.command()
        self.send(obj);self.step()
        reply = self.send(obj)
        self.assertEqual(reply["ack"]["status"], "APPLIED")
        self.assertEqual(reply["ack"]["applied_tick"], 1)
        self.step();self.assertEqual(self.fdm.writes, 1)

    def test_conflict_does_not_replace_original(self):
        self.send(self.command());self.step()
        self.assertEqual(self.send(self.command(value=.5))["console"], "DUPLICATE_CONFLICT")
        self.assertEqual(self.send(self.command())["ack"]["status"], "APPLIED")

    def test_stop_bypasses_full_and_busy_mailbox_without_physics(self):
        for _ in range(16):self.assertEqual(self.console.offer(b'{"action":"STATUS"}'), "QUEUED_NOT_VALIDATED")
        self.assertEqual(self.console.offer(b'{"action":"STATUS"}'), "FULL")
        with self.console._lock:
            thread = threading.Thread(target=lambda:self.console.offer(b'{"action":"STOP"}'))
            thread.start();thread.join(1);self.assertFalse(thread.is_alive())
            self.assertTrue(self.fixture.control.inbox.revoked)
            self.clock.now += TICK_NS
            self.assertFalse(self.console.pump())
        self.assertEqual(self.engine.state, "COMPLETE")
        self.assertEqual(self.fdm.steps, 0)

    def test_pause_pump_maintains_watchdog_and_never_rearms_after_resume(self):
        obj = self.command();self.send(obj);self.step()
        self.engine.pause();self.clock.now += TICK_NS;self.console.pump()
        self.engine.resume()
        self.assertEqual(self.send(obj)["ack"]["status"], "APPLIED")
        self.assertEqual(self.send(self.command(command_id="new", sequence=2))["console"], "NOT_ADMITTED")
        self.assertEqual(self.fdm.writes, 1)

    def test_failed_ingress_is_retained_and_not_resubmitted(self):
        with patch.object(self.fixture.control, "offer", return_value="BUSY") as offer:
            self.assertEqual(self.send(self.command())["console"], "BUSY")
            self.assertEqual(self.send(self.command())["console"], "BUSY")
            self.assertEqual(offer.call_count, 1)
        self.step();self.assertEqual(self.fdm.writes, 0)

    def test_history_count_and_bytes_exhaustion_stop_without_eviction(self):
        for attr, limit in (("MAX_HISTORY", 1), ("MAX_HISTORY_BYTES", 1)):
            with self.subTest(attr=attr):
                if self.engine.state != "RUNNING":self.setUp()
                setattr(self.console, attr, limit)
                self.send(self.command())
                if attr == "MAX_HISTORY":
                    self.send(self.command(command_id="second", sequence=2))
                self.assertEqual(self.engine.state, "COMPLETE")
                self.assertEqual(self.fdm.writes, 0)

    def test_strict_fields_ranges_json_and_no_allocation_for_queries(self):
        for raw in (b'{}', b'[]', b'{"action":"STATUS","extra":1}', b'{"action":"STATUS","action":"STOP"}',
                    b'\xff', b'{"action": NaN}'):
            self.console.offer(raw);self.console.pump()
        for obj in (self.command(value=True), self.command(value=2), self.command(control="rudder"),
                    self.command(sequence=True), self.command(principal="attacker")):
            self.assertEqual(self.send(obj)["console"], "INVALID")
        for _ in range(50):self.send(dict(action="LOOKUP", command_id="unknown"))
        self.assertEqual(len(self.console._history), 0)
        self.assertEqual(self.fdm.writes, 0)

    def test_oversize_and_eof_policy_stop_without_processing_queue(self):
        self.console.offer(encode(self.command()))
        self.assertEqual(self.console.offer(b'x'*4097), "STOP_REQUESTED_INPUT_BOUND")
        self.console.pump();self.assertEqual(self.fdm.writes, 0)
        self.assertEqual(self.engine.state, "COMPLETE")

    def test_service_is_owner_only_producer_offer_is_thread_safe(self):
        errors = []
        def producer():
            self.console.offer(b'{"action":"STATUS"}')
            try:self.console.service()
            except RuntimeError:errors.append(True)
        thread=threading.Thread(target=producer);thread.start();thread.join(1)
        self.assertEqual(errors, [True]);self.console.pump()
        self.assertEqual(c.decode(self.console.latest())[0]["state"], "RUNNING")

    def test_uncertain_outcome_is_never_reported_as_ack_or_reexecuted(self):
        self.send(self.command())
        with patch.object(self.fixture.control, "lookup_outcome", side_effect=RuntimeError("uncertain")):
            self.assertEqual(self.send(self.command())["console"], "UNCERTAIN_NO_RETRY")
        self.assertEqual(self.fdm.writes, 0)

    def test_lifecycle_unsupported_and_status_is_not_a_live_frame(self):
        for action in ("PAUSE", "RESUME", "START", "REARM"):
            self.assertEqual(self.send(dict(action=action))["console"], "UNSUPPORTED")
        self.engine.pause()
        self.assertEqual(self.send(dict(action="STATUS"))["state"], "PAUSED")
        self.assertEqual(self.engine.state, "PAUSED")

    def test_transport_fault_revokes_and_fails_without_success_result(self):
        self.console.offer(encode(self.command()))
        self.console.request_stop(failed=True);self.console.pump()
        self.assertEqual(self.engine.state, "FAILED")
        self.assertEqual(self.fdm.writes, 0)
        self.assertEqual(c.decode(self.console.latest())["console"], "TERMINAL")


if __name__ == "__main__":unittest.main()
