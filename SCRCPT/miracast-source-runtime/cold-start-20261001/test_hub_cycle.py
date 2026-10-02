import io
import itertools
import unittest
from unittest import mock
import hub_cycle


class HubCycleTests(unittest.TestCase):
    def run_fake(self, failure=None):
        events = []
        count = [0]

        class Console:
            def __init__(self, number): self.number = number; self.sent = False
            def __enter__(self): return self
            def __exit__(self, *args): self.close()
            def reset_input_buffer(self): pass
            def close(self): events.append("close" + str(self.number))
            def read(self, size):
                if failure == "read" and self.number == 2:
                    raise hub_cycle.serial.SerialException("stale handle")
                if self.sent: return b""
                self.sent = True
                return b"Linux version test\n"

        def open_serial(*args, **kwargs):
            count[0] += 1
            events.append("open" + str(count[0]))
            if failure == "probe": raise hub_cycle.serial.SerialException("busy")
            return Console(count[0])

        def command(action, timeout):
            events.append(action)
            if failure == "ack": raise RuntimeError("ack missing")

        hub = mock.Mock()
        hub.send_command.side_effect = command
        clock = itertools.count(0, 0.1)
        with mock.patch.object(hub_cycle.serial, "Serial", side_effect=open_serial), \
             mock.patch.object(hub_cycle.time, "sleep"), \
             mock.patch.object(hub_cycle.time, "monotonic", side_effect=lambda: next(clock)):
            if failure in ("probe", "ack"):
                with self.assertRaises((RuntimeError, hub_cycle.serial.SerialException)):
                    hub_cycle.run_cycle(hub, "COM4", 5, 1, io.BytesIO())
                result = None
            else:
                result = hub_cycle.run_cycle(hub, "COM4", 5, 1, io.BytesIO())
        return events, result

    def test_old_handle_closed_and_new_opened_after_on(self):
        events, result = self.run_fake()
        self.assertLess(events.index("close1"), events.index("OFF"))
        self.assertLess(events.index("ON"), events.index("open2"))
        self.assertTrue(result["boot_marker"])

    def test_read_failure_reopens_without_repeating_power(self):
        events, result = self.run_fake("read")
        self.assertEqual(events.count("OFF"), 1)
        self.assertEqual(events.count("ON"), 1)
        self.assertEqual(result["console_reopens"], 1)
        self.assertTrue(result["boot_marker"])

    def test_busy_console_prevents_power_action(self):
        events, _ = self.run_fake("probe")
        self.assertNotIn("OFF", events)

    def test_missing_off_ack_prevents_on(self):
        events, _ = self.run_fake("ack")
        self.assertNotIn("ON", events)


if __name__ == "__main__": unittest.main()
