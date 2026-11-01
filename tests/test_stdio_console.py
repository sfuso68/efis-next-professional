import threading
import time
import unittest
from unittest.mock import Mock, patch

from tools.simulation.owner_console import StdioChannel, publish_snapshot
from tools.simulation import session_contract as c
from tools.simulation.session_engine import encode


class StdioConsoleTests(unittest.TestCase):
    def setUp(self):
        self.console = Mock()
        self.console.offer.return_value = "QUEUED_NOT_VALIDATED"
        self.channel = StdioChannel(self.console, 90, 91)

    def test_reader_bounded_partial_lines_and_eof_stop(self):
        with patch("os.read", side_effect=[b'{"action":', b'"STATUS"}\n{"action":"STOP"}\n', b'']):
            self.channel._read()
        self.assertEqual(self.console.offer.call_count, 2)
        self.console.offer.assert_any_call(b'{"action":"STATUS"}')
        self.console.request_stop.assert_called_once()

    def test_reader_oversize_stops_before_allocating_unbounded_line(self):
        calls=[]
        def read(fd, size):calls.append(size);return b'x'*size
        with patch("os.read", side_effect=read):self.channel._read()
        self.assertEqual(sum(calls), 4097)
        self.console.offer.assert_not_called();self.console.request_stop.assert_called_once()

    def test_read_and_write_errors_stop_not_silent_thread_death(self):
        with patch("os.read", side_effect=OSError("read failed")):self.channel._read()
        self.channel.publish(b'hello\n')
        with patch("os.write", side_effect=OSError("write failed")):self.channel._write()
        self.assertEqual(self.console.request_stop.call_count, 2)
        self.console.request_stop.assert_called_with(failed=True)

    def test_latest_only_output_and_partial_writes(self):
        self.channel.publish(b'old\n');self.channel.publish(b'latest\n')
        self.assertEqual(self.channel.overwritten, 1)
        self.channel._end.set();output=[]
        def write(fd, data):output.append(bytes(data[:2]));return min(2,len(data))
        with patch("os.write", side_effect=write):self.channel._write()
        self.assertEqual(b''.join(output), b'latest\n')

    def test_blocked_output_cannot_hold_owner_or_finite_join(self):
        entered, release = threading.Event(), threading.Event()
        def blocked(fd, data):entered.set();release.wait(2);return len(data)
        with patch("os.write", side_effect=blocked):
            self.channel.publish(b'blocked\n');self.channel.writer.start()
            try:
                self.assertTrue(entered.wait(1))
                for _ in range(100):self.channel.publish(b'new\n')
                # close expects a started reader; no read is needed in this test.
                self.channel.reader = Mock()
                started=time.monotonic();self.channel.close()
                self.assertLess(time.monotonic()-started, .3)
                self.assertTrue(self.channel.writer.is_alive())
                self.assertGreaterEqual(self.channel.overwritten, 99)
            finally:release.set();self.channel.writer.join(1)

    def test_output_validation_and_busy_lock_are_nonblocking(self):
        for packet in ("mutable", b'x'*65537):
            with self.assertRaises(ValueError):self.channel.publish(packet)
        with self.channel._lock:self.channel.publish(b'dropped\n')
        self.assertEqual(self.channel.overwritten, 1)
        self.assertIsNone(self.channel._packet)

    def test_partial_thread_startup_still_closes_started_writer(self):
        with patch.object(self.channel.reader, "start", side_effect=RuntimeError("thread start")):
            with self.assertRaisesRegex(RuntimeError, "thread start"):self.channel.start()
        self.channel.close()
        self.assertFalse(self.channel.writer.is_alive())

    def test_invalid_and_full_ingress_are_visible_in_bounded_snapshot(self):
        self.console.offer.side_effect=["INVALID", "FULL", "BUSY"]
        with patch("os.read", side_effect=[b'bad\nbad\nbad\n', b'']):self.channel._read()
        self.assertEqual(self.channel.ingress, (3,"BUSY"))

    def test_reply_and_ingress_update_share_one_publication(self):
        self.channel.ingress=(1,"QUEUED_NOT_VALIDATED")
        reply=[dict(console="LOOKUP",command_id="input-1",ack=dict(status="APPLIED"))]
        with patch.object(self.channel,"publish") as publish:
            self.assertEqual(publish_snapshot(self.channel,encode(reply),0),1)
        publish.assert_called_once()
        result=c.decode(publish.call_args.args[0])
        self.assertEqual(result["replies"],reply)
        self.assertEqual(result["ingress"]["generation"],1)


if __name__ == "__main__":unittest.main()
