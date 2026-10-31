#!/usr/bin/env python3
"""Parse compact UART measurement captures for host-side regression tests."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import re
from typing import Iterable


TIMESTAMP_RE = re.compile(r"@@\s+(?P<seconds>\d+)\.(?P<millis>\d{3})")
KV_RE = re.compile(r"(?P<key>[A-Za-z][A-Za-z0-9_]*)=(?P<value>[0-9A-Fa-f]+)")

COMPACT_SCHEMAS = {
    "O": ("r", "raw", "inst", "disp", "ax", "ah", "ph", "ps", "dc", "tw"),
    "D": ("m", "r", "st", "raw", "inst", "disp", "dc", "tw", "opt", "buzz"),
    "C": (
        "m",
        "r",
        "st",
        "raw",
        "inst",
        "disp",
        "dc",
        "tw",
        "opt",
        "threshold",
        "decision",
        "buzz",
    ),
}


@dataclass(frozen=True)
class ReplayFrame:
    timestamp_ms: int
    record: str
    fields: dict[str, int]

    def require(self, name: str) -> int:
        if name not in self.fields:
            raise AssertionError(f"{self.record} frame is missing {name}")
        return self.fields[name]


def parse_line(line: str) -> ReplayFrame | None:
    timestamp = TIMESTAMP_RE.search(line)
    if timestamp is None:
        return None

    suffix = line[timestamp.end() :].strip()
    record_text = suffix.split(maxsplit=1)[0]
    fields = {
        match.group("key"): int(match.group("value"), 16)
        for match in KV_RE.finditer(line)
    }
    if not fields:
        parts = record_text.split(",")
        schema = COMPACT_SCHEMAS.get(parts[0])
        if schema is None or len(parts) != len(schema) + 1:
            return None
        try:
            fields = {
                name: int(value, 16)
                for name, value in zip(schema, parts[1:], strict=True)
            }
        except ValueError:
            return None
        if parts[0] == "O":
            fields["m"] = 0x09

    record = record_text.split(",", maxsplit=1)[0]
    seconds = int(timestamp.group("seconds"), 10)
    millis = int(timestamp.group("millis"), 10)
    return ReplayFrame(seconds * 1000 + millis, record, fields)


def load_replay(path: str | Path) -> list[ReplayFrame]:
    frames = []
    for line in Path(path).read_text(encoding="utf-8").splitlines():
        frame = parse_line(line)
        if frame is not None:
            frames.append(frame)
    if not frames:
        raise AssertionError(f"{path} contains no replay frames")
    return frames


def transition_sequence(frames: Iterable[ReplayFrame], field: str) -> list[int]:
    sequence: list[int] = []
    for frame in frames:
        if field not in frame.fields:
            continue
        value = frame.fields[field]
        if not sequence or sequence[-1] != value:
            sequence.append(value)
    return sequence


def assert_monotonic(frames: Iterable[ReplayFrame]) -> None:
    previous = -1
    for frame in frames:
        if frame.timestamp_ms < previous:
            raise AssertionError(
                f"timestamp moved backwards: {frame.timestamp_ms} < {previous}"
            )
        previous = frame.timestamp_ms


def assert_mode_and_range(
    frames: Iterable[ReplayFrame], expected_mode: int, maximum_range: int
) -> None:
    for frame in frames:
        mode = frame.require("m")
        range_value = frame.require("r")
        if mode != expected_mode:
            raise AssertionError(f"unexpected mode {mode:02X}, expected {expected_mode:02X}")
        if range_value > maximum_range:
            raise AssertionError(
                f"range {range_value:02X} exceeds {maximum_range:02X}"
            )


def signed32(value: int) -> int:
    value &= 0xFFFFFFFF
    return value - 0x100000000 if value & 0x80000000 else value
