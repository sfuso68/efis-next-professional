import copy
import threading
import unittest

from tools.simulation import session_contract as c
from tools.simulation.command_queue import CommandQueue
from tools.simulation.session_engine import encode


class CommandQueueTests(unittest.TestCase):
    def setUp(self):
        vector = c.decode((c.ROOT / "tests/data/protocols/sil-session-v1-vectors.json").read_bytes())
        self.manifest, self.command = vector["manifest"], vector["command"]
        self.now = 1000
        self.q = CommandQueue(self.manifest, principal="instructor", lease_id="lease-001",
                              lease_until_tick=100, lease_duration_ns=1_000_000_000,
                              clock=lambda: self.now)

    def request(self, seq=1, **changes):
        r = copy.deepcopy(self.command)
        r.update(sequence=seq, command_id=f"cmd-{seq}")
        r.update(changes)
        return r

    def submit(self, r=None, tick=0, principal="instructor"):
        return self.q.submit(encode(self.request() if r is None else r),
                             authenticated_principal=principal, current_tick=tick)

    def test_admission_and_one_time_immutable_dispatch(self):
        r = self.request()
        ack = self.submit(r)
        self.assertEqual(ack["status"], "QUEUED")
        self.assertIsNone(ack["applied_tick"])
        r["args"]["value"] = -1
        self.assertEqual(self.q.take_due(current_tick=0), ())
        due = self.q.take_due(current_tick=1)
        self.assertIsInstance(due[0], bytes)
        self.assertEqual(c.decode(due[0])["args"]["value"], 0.25)
        self.assertEqual(self.q.take_due(current_tick=2), ())

    def test_retry_before_and_after_dispatch_never_requeues(self):
        ack = self.submit()
        self.assertEqual(self.submit(), ack)
        self.q.take_due(current_tick=1)
        self.assertEqual(self.submit(tick=2), ack)
        self.assertEqual(self.q.counts["pending"], 0)
        ack["status"] = "APPLIED"
        self.assertEqual(self.submit(tick=3)["status"], "QUEUED")

    def test_conflict_and_sequence_reuse(self):
        self.submit()
        altered = self.request(args={"control":"aileron", "value":-0.2})
        self.assertEqual(self.submit(altered)["reason"], "DUPLICATE_CONFLICT")
        self.assertEqual(self.submit(self.request(command_id="another"))["reason"], "INVALID")
        self.assertEqual(self.q.counts["pending"], 1)

    def test_wrong_identity_epoch_lease_or_json_never_queues(self):
        for changes in ({"epoch":1}, {"session_id":"other"}, {"lease_id":"other"},
                        {"authority_id":"other"}, {"flight_approval":True}, {"endpoint":"x"}):
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                self.submit(self.request(**changes))
        with self.assertRaises(ValueError): self.submit(principal="stranger")
        for raw in (b'{"x":1,"x":2}', b'{}', b'NaN', b' '*65537):
            with self.assertRaises((ValueError, KeyError)):
                self.q.submit(raw, authenticated_principal="instructor", current_tick=0)
        self.assertEqual(self.q.counts["history"], 0)

    def test_wall_lease_expires_even_without_simulation_ticks(self):
        self.submit()
        self.now += 1_000_000_000
        self.assertEqual(self.q.take_due(current_tick=1), ())
        self.assertEqual(self.submit(tick=1)["reason"], "NO_LEASE")
        self.assertEqual(self.submit(self.request(2), tick=1)["reason"], "NO_LEASE")

    def test_tick_expiry_and_horizon(self):
        self.assertEqual(self.submit(self.request(apply_tick=101, expires_tick=101))["reason"], "INVALID")
        self.submit(self.request(2))
        self.assertEqual(self.q.take_due(current_tick=21), ())
        self.assertEqual(self.submit(self.request(2), tick=21)["reason"], "EXPIRED")

    def test_pending_capacity_and_revoke_independent_of_capacity(self):
        for seq in range(1, 17): self.assertEqual(self.submit(self.request(seq))["status"], "QUEUED")
        self.assertEqual(self.submit(self.request(17))["status"], "REJECTED")
        self.assertEqual(self.q.counts["pending"], 16)
        self.q.revoke()
        self.assertEqual(self.q.take_due(current_tick=1), ())

    def test_history_limit_no_eviction_fails_closed(self):
        self.q.MAX_HISTORY = 2
        self.submit()
        self.submit(self.request(2))
        with self.assertRaisesRegex(ValueError, "history capacity"):
            self.submit(self.request(3))
        self.assertTrue(self.q.closed)
        self.assertEqual(self.q.counts["history"], 2)
        self.assertEqual(self.q.take_due(current_tick=1), ())

    def test_byte_limit_fails_closed(self):
        self.q.MAX_BYTES = len(encode(self.request()))
        self.submit()
        with self.assertRaises(ValueError): self.submit(self.request(2))
        self.assertTrue(self.q.closed)
        self.assertEqual(self.q.counts["pending"], 0)

    def test_tick_and_clock_reversal_rejected(self):
        self.submit(self.request(apply_tick=5))
        self.q.take_due(current_tick=2)
        with self.assertRaises(ValueError): self.q.take_due(current_tick=1)
        self.assertEqual(self.q.counts["pending"], 0)
        self.now -= 1
        with self.assertRaises(ValueError): self.q.take_due(current_tick=2)
        self.assertTrue(self.q.closed)
        self.now = 1000
        self.assertEqual(self.q.take_due(current_tick=5), ())

    def test_lease_tick_boundary_and_duration_bounds(self):
        self.submit(self.request(apply_tick=100, expires_tick=100))
        self.assertEqual(len(self.q.take_due(current_tick=100)), 1)
        self.q.take_due(current_tick=101)
        self.assertTrue(self.q.closed)
        for duration in (0, True, 5_000_000_001):
            with self.assertRaises(ValueError):
                CommandQueue(self.manifest, principal="instructor", lease_id="new",
                             lease_until_tick=100, lease_duration_ns=duration)

    def test_order_and_closed_queue(self):
        self.submit(self.request(1, apply_tick=5))
        self.submit(self.request(2, apply_tick=2))
        values = self.q.take_due(current_tick=5)
        self.assertEqual([c.decode(v)["sequence"] for v in values], [2, 1])
        self.q.revoke()
        self.assertEqual(self.submit(self.request(3), tick=5)["reason"], "NO_LEASE")

    def test_owner_thread_enforced(self):
        failures = []
        def attempt():
            try: self.q.revoke()
            except RuntimeError as exc: failures.append(str(exc))
        thread = threading.Thread(target=attempt)
        thread.start(); thread.join()
        self.assertEqual(len(failures), 1)
        self.assertFalse(self.q.closed)


if __name__ == "__main__":
    unittest.main()
