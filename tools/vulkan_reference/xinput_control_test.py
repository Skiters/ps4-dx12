"""Cached XInput readback checks; no controller, driver or game required."""
import ctypes as C
import unittest

from xinput_control import Report, State, wait_report


class ReportVisibilityTests(unittest.TestCase):
    def test_delayed_neutral(self):
        calls = 0

        def get(index, pointer):
            nonlocal calls
            self.assertEqual(index, 2)
            calls += 1
            state = C.cast(pointer, C.POINTER(State)).contents
            state.packet = calls
            state.gamepad = Report(4096 if calls < 3 else 0)
            return 0

        result, state, attempts = wait_report(get, 2, Report())
        self.assertEqual((result, state.gamepad.buttons, attempts), (0, 0, 3))

    def test_persistent_wrong_report_is_not_success(self):
        def get(index, pointer):
            C.cast(pointer, C.POINTER(State)).contents.gamepad = Report(4096)
            return 0

        result, state, attempts = wait_report(get, 0, Report(), timeout=0)
        self.assertEqual((result, state.gamepad.buttons, attempts), (0, 4096, 1))

    def test_disconnected_neutral_is_not_success(self):
        result, _, attempts = wait_report(lambda index, pointer: 1167, 0, Report(), timeout=0)
        self.assertEqual((result, attempts), (1167, 1))


if __name__ == "__main__":
    unittest.main()
