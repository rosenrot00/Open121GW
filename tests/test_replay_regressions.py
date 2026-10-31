#!/usr/bin/env python3

from pathlib import Path
import sys
import unittest


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from replay_regression import (  # noqa: E402
    assert_mode_and_range,
    assert_monotonic,
    load_replay,
    signed32,
    transition_sequence,
)


FIXTURES = ROOT / "tests" / "fixtures"


class ReplayRegressionTests(unittest.TestCase):
    def test_ohms_100r_entry_reaches_precision_without_intermediate_display(self):
        frames = load_replay(FIXTURES / "ohms_100r_entry.log")

        assert_monotonic(frames)
        assert_mode_and_range(frames, expected_mode=0x09, maximum_range=6)
        self.assertEqual([4, 3, 1], transition_sequence(frames, "r"))
        self.assertEqual([0, 2], transition_sequence(frames, "ph"))

        search_frames = [frame for frame in frames if frame.require("ph") == 0]
        self.assertTrue(search_frames)
        self.assertTrue(all(frame.require("disp") == 0 for frame in search_frames))

        precision = [frame for frame in frames if frame.require("ph") == 2]
        self.assertTrue(precision)
        self.assertLessEqual(precision[0].timestamp_ms, 450)
        self.assertTrue(any(9500 <= signed32(frame.require("disp")) <= 11000
                            for frame in precision))

    def test_stock_cap_open_transition_keeps_frontend_contract(self):
        frames = load_replay(FIXTURES / "cap_stock_open_transition.log")

        assert_monotonic(frames)
        assert_mode_and_range(frames, expected_mode=0x0C, maximum_range=5)
        self.assertEqual([1, 0], transition_sequence(frames, "r"))
        self.assertTrue(all(frame.require("st") == 1 for frame in frames))
        self.assertTrue(all(frame.require("r1e") == 1 for frame in frames))
        self.assertTrue(all(frame.require("r1f") == 1 for frame in frames))
        self.assertTrue(all(frame.require("tw") == 0xE000 for frame in frames))

        low_range = [frame for frame in frames if frame.require("r") == 0]
        self.assertTrue(low_range)
        self.assertEqual(0, low_range[0].require("val"))

    def test_diode_short_buzzer_follows_fast_measurement_path(self):
        frames = load_replay(FIXTURES / "diode_short_transition.log")

        assert_monotonic(frames)
        assert_mode_and_range(frames, expected_mode=0x0B, maximum_range=0)
        self.assertTrue(all(frame.record == "D" for frame in frames))
        self.assertTrue(all(frame.require("st") == 4 for frame in frames))
        self.assertTrue(all(frame.require("tw") == 0xE000 for frame in frames))
        self.assertEqual([0, 1, 0], transition_sequence(frames, "buzz"))

        first_beep = next(index for index, frame in enumerate(frames)
                          if frame.require("buzz") == 1)
        self.assertLessEqual(
            frames[first_beep].timestamp_ms - frames[first_beep - 1].timestamp_ms,
            70,
        )
        first_release = next(
            index for index in range(first_beep + 1, len(frames))
            if frames[index].require("buzz") == 0
        )
        self.assertLessEqual(
            frames[first_release].timestamp_ms - frames[first_release - 1].timestamp_ms,
            70,
        )

    def test_continuity_dn300_hysteresis_survives_open_close_cycles(self):
        frames = load_replay(FIXTURES / "continuity_dn300_cycles.log")

        assert_monotonic(frames)
        assert_mode_and_range(frames, expected_mode=0x0A, maximum_range=0)
        self.assertTrue(all(frame.record == "C" for frame in frames))
        self.assertTrue(all(frame.require("st") == 4 for frame in frames))
        self.assertTrue(all(frame.require("tw") == 0xF000 for frame in frames))
        self.assertTrue(all(frame.require("opt") == 2 for frame in frames))
        self.assertEqual([1, 0, 1, 0, 1], transition_sequence(frames, "buzz"))
        self.assertTrue(
            all(frame.require("decision") == frame.require("buzz") for frame in frames)
        )

        active_thresholds = {
            frame.require("threshold")
            for frame in frames
            if frame.require("buzz") == 1
        }
        inactive_thresholds = {
            frame.require("threshold")
            for frame in frames
            if frame.require("buzz") == 0
        }
        self.assertEqual({0x7724}, active_thresholds)
        self.assertEqual({0x7530}, inactive_thresholds)

    def test_ohms_fast_range_uses_fast_timing_only_while_searching(self):
        frames = load_replay(FIXTURES / "ohms_fast_range_transition.log")

        assert_monotonic(frames)
        assert_mode_and_range(frames, expected_mode=0x09, maximum_range=6)
        self.assertTrue(all(frame.record == "O" for frame in frames))
        self.assertEqual([1, 2, 3, 4, 5], transition_sequence(frames, "r"))
        self.assertEqual([2, 0, 1, 2], transition_sequence(frames, "ph"))

        search = [frame for frame in frames if frame.require("ph") == 0]
        precision = [frame for frame in frames if frame.require("ph") == 2]
        self.assertTrue(search)
        self.assertTrue(precision)
        self.assertTrue(all(frame.require("tw") == 0xF000 for frame in search))
        self.assertTrue(all(frame.require("tw") == 0xE000 for frame in precision))


if __name__ == "__main__":
    unittest.main()
