import copy
import threading
import unittest
from tools.simulation import session_contract as c
from tools.simulation.command_queue import CommandQueue
from tools.simulation.control_inbox import ControlInbox
from tools.simulation.session_engine import encode


class ControlInboxTests(unittest.TestCase):
    def setUp(self):
        v=c.decode((c.ROOT/"tests/data/protocols/sil-session-v1-vectors.json").read_bytes())
        self.m,self.request=v["manifest"],v["command"];self.now=1000
        self.commands=CommandQueue(self.m,principal="instructor",lease_id="lease-001",
            lease_until_tick=100,lease_duration_ns=1_000_000_000,clock=lambda:self.now)
        self.inbox=ControlInbox()

    def raw(self,seq=1):
        request=copy.deepcopy(self.request);request.update(sequence=seq,command_id=f"cmd-{seq}")
        return encode(request)

    def offer(self,seq=1,principal="instructor"):
        return self.inbox.offer(self.raw(seq),principal=principal)

    def test_cross_thread_offer_owner_admission_and_batch_budget(self):
        thread=threading.Thread(target=lambda:[self.offer(n) for n in range(1,7)])
        thread.start();thread.join()
        self.assertEqual(self.commands.counts["history"],0)
        first=self.inbox.service(self.commands,current_tick=0)
        self.assertEqual(len(first),4)
        self.assertTrue(all(r["ack"]["status"]=="QUEUED" for r in first))
        self.assertEqual(len(self.inbox.service(self.commands,current_tick=0)),2)

    def test_immutable_types_and_sizes_only(self):
        for value in ("text",bytearray(b"x"),b"",b"x"*65537):
            with self.assertRaises(ValueError):self.inbox.offer(value,principal="instructor")

    def test_item_and_byte_full_diagnostics_do_not_drop_old_commands(self):
        for n in range(1,17):self.assertEqual(self.offer(n),"QUEUED_NOT_VALIDATED")
        self.assertEqual(self.offer(17),"FULL")
        self.assertEqual(len(self.inbox.service(self.commands,current_tick=0)),4)
        other=ControlInbox();other.MAX_BYTES=len(self.raw())
        self.assertEqual(other.offer(self.raw(),principal="instructor"),"QUEUED_NOT_VALIDATED")
        self.assertEqual(other.offer(self.raw(2),principal="instructor"),"FULL")

    def test_contended_lock_never_blocks_owner_or_producer(self):
        self.inbox._lock.acquire()
        try:
            self.assertEqual(self.offer(),"BUSY")
            self.assertEqual(self.inbox.service(self.commands,current_tick=0),({"ingress":"BUSY"},))
        finally:self.inbox._lock.release()

    def test_priority_revoke_with_full_and_contended_fifo(self):
        for n in range(1,17):self.offer(n)
        self.inbox._lock.acquire()
        try:
            thread=threading.Thread(target=self.inbox.revoke);thread.start();thread.join(timeout=1)
            self.assertFalse(thread.is_alive())
            self.assertEqual(self.inbox.service(self.commands,current_tick=0),({"ingress":"REVOKED"},))
            self.assertTrue(self.commands.closed)
        finally:self.inbox._lock.release()
        self.assertEqual(self.offer(),"REVOKED")

    def test_idle_pause_service_expires_wall_lease_without_tick_advance(self):
        self.offer();self.inbox.service(self.commands,current_tick=0)
        self.now+=1_000_000_000
        self.assertEqual(self.inbox.service(self.commands,current_tick=0),({"ingress":"NO_LEASE"},))
        self.assertEqual(self.commands.counts["pending"],0)
        self.assertTrue(self.commands.closed)

    def test_authentication_is_trusted_caller_context_not_payload(self):
        self.offer(principal="stranger")
        self.inbox.offer(b'{"bad":true}',principal="instructor")
        replies=self.inbox.service(self.commands,current_tick=0)
        self.assertTrue(all(x["ingress"]=="REJECTED_INVALID" for x in replies))
        self.assertEqual(self.commands.counts["history"],0)

    def test_revoke_after_dispatch_prevents_authorization(self):
        self.offer();self.inbox.service(self.commands,current_tick=0)
        request=c.decode(self.commands.take_due(current_tick=1)[0])
        self.inbox.revoke()
        self.assertFalse(self.inbox.authorize_dispatch(self.commands,request,current_tick=1))
        self.assertEqual(self.commands.submit(self.raw(),authenticated_principal="instructor",current_tick=1)["reason"],"NO_LEASE")

    def test_cached_applied_outcome_retry_does_not_requeue(self):
        raw=self.raw();self.inbox.offer(raw,principal="instructor")
        self.inbox.service(self.commands,current_tick=0)
        request=c.decode(self.commands.take_due(current_tick=1)[0])
        self.assertTrue(self.inbox.authorize_dispatch(self.commands,request,current_tick=1))
        ack=self.commands._ack(request);ack.update(status="APPLIED",applied_tick=1)
        self.commands.record_outcome(request,ack,current_tick=1)
        self.inbox.offer(raw,principal="instructor")
        self.assertEqual(self.inbox.service(self.commands,current_tick=1)[0]["ack"],ack)
        self.assertEqual(self.commands.take_due(current_tick=2),())

    def test_unearned_and_duplicate_outcomes_rejected(self):
        request=c.decode(self.raw());ack=self.commands._ack(request)
        ack.update(status="APPLIED",applied_tick=1)
        with self.assertRaises(ValueError):self.commands.record_outcome(request,ack,current_tick=1)
        self.offer();self.inbox.service(self.commands,current_tick=0);self.commands.take_due(current_tick=1)
        invalid=dict(ack,applied_tick=2)
        with self.assertRaises(ValueError):self.commands.record_outcome(request,invalid,current_tick=1)
        self.commands.record_outcome(request,ack,current_tick=1)
        with self.assertRaises(ValueError):self.commands.record_outcome(request,ack,current_tick=1)

    def test_pending_plus_dispatched_share_capacity(self):
        for n in range(1,17):self.commands.submit(self.raw(n),authenticated_principal="instructor",current_tick=0)
        self.assertEqual(len(self.commands.take_due(current_tick=1)),16)
        request=c.decode(self.raw(17));request.update(apply_tick=2)
        self.assertEqual(self.commands.submit(encode(request),authenticated_principal="instructor",current_tick=1)["status"],"REJECTED")

    def test_inbox_service_on_wrong_thread_is_forbidden(self):
        failures=[]
        def work():
            try:self.inbox.service(self.commands,current_tick=0)
            except RuntimeError as exc:failures.append(str(exc))
        thread=threading.Thread(target=work);thread.start();thread.join()
        self.assertEqual(len(failures),1)


    def test_terminal_lookup_survives_expiry_and_revocation_without_admission(self):
        self.offer();self.inbox.service(self.commands,current_tick=0)
        request=c.decode(self.commands.take_due(current_tick=1)[0])
        ack=self.commands._ack(request);ack.update(status="APPLIED",applied_tick=1)
        self.commands.record_outcome(request,ack,current_tick=1)
        self.now+=1_000_000_000
        self.inbox.service(self.commands,current_tick=1);self.inbox.revoke()
        self.assertEqual(self.inbox.lookup_outcome(self.commands,self.raw(),principal="instructor"),ack)
        self.assertIsNone(self.inbox.lookup_outcome(self.commands,self.raw(2),principal="instructor"))
        self.assertEqual(self.commands.counts["history"],1)
        self.assertEqual(self.commands.take_due(current_tick=1),())
        with self.assertRaises(ValueError):
            self.inbox.lookup_outcome(self.commands,self.raw(),principal="stranger")

    def test_rejected_lookup_survives_revocation(self):
        self.offer();self.inbox.service(self.commands,current_tick=0)
        self.inbox.revoke();self.inbox.service(self.commands,current_tick=0)
        ack=self.inbox.lookup_outcome(self.commands,self.raw(),principal="instructor")
        self.assertEqual((ack["status"],ack["reason"]),("REJECTED","NO_LEASE"))

    def test_outcome_advances_monotonic_tick(self):
        self.offer();self.inbox.service(self.commands,current_tick=0)
        request=c.decode(self.commands.take_due(current_tick=1)[0])
        ack=self.commands._ack(request);ack.update(status="APPLIED",applied_tick=5)
        self.commands.record_outcome(request,ack,current_tick=5)
        with self.assertRaises(ValueError):self.commands.maintain(current_tick=4)
        self.assertTrue(self.commands.closed)

    def test_dispatched_request_expiry_cannot_become_applied(self):
        self.offer();self.inbox.service(self.commands,current_tick=0)
        request=c.decode(self.commands.take_due(current_tick=1)[0])
        tick=request["expires_tick"]+1
        self.commands.maintain(current_tick=tick)
        ack=self.commands._ack(request);ack.update(status="APPLIED",applied_tick=1)
        with self.assertRaises(ValueError):self.commands.record_outcome(request,ack,current_tick=tick)
        self.assertEqual(self.commands.lookup_outcome(self.raw(),authenticated_principal="instructor")["reason"],"EXPIRED")


if __name__=="__main__":unittest.main()
