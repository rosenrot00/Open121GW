#!/usr/bin/env python3
import argparse
import os
import re
import time

from capture_rotary_modes import close_uart, command, open_uart, read_lines


MODE_BUTTON = 0x01
RANGE_BUTTON = 0x10
ROTARY_MODES = {
    0x01: (0x00,),
    0x04: (0x03, 0x04, 0x05),
}
MANUAL_RANGE_COUNTS = {
    0x00: 0,
    0x03: 2,
    0x04: 2,
    0x05: 0,
}
SNAPSHOT_COMMANDS = ("UAPP", "UP10D", "UFECFG", "ULCD", "UFE")


def send(report, fd, text, delay=0.08, read_time=0.25):
    report.append(f">> {text}")
    lines = command(fd, text, delay=delay, read_time=read_time, flush_before=True)
    report.extend(f"<< {line}" for line in lines)
    return lines


def parse_field(lines, name):
    pattern = re.compile(rf"\b{name}=([0-9A-Fa-f]+)")
    for line in lines:
        match = pattern.search(line)
        if match:
            return int(match.group(1), 16)
    return None


def read_app(report, fd):
    return send(report, fd, "UAPP", delay=0.05, read_time=0.18)


def click(report, fd, mask, settle_s):
    send(report, fd, f"UXBTN {mask:02X}", delay=0.05, read_time=0.15)
    send(report, fd, "UXBTN 00", delay=0.05, read_time=0.15)
    time.sleep(settle_s)


def wait_for_mode(report, fd, wanted, timeout_s=2.0):
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        lines = read_app(report, fd)
        if parse_field(lines, "mode") == wanted:
            return True
        time.sleep(0.08)
    return False


def snapshot(report, fd, label):
    report.append(f"\n## {label}")
    for text in SNAPSHOT_COMMANDS:
        send(report, fd, text, delay=0.06, read_time=0.30)


def normalize_mode(report, fd, wanted, settle_s):
    for _ in range(4):
        lines = read_app(report, fd)
        if parse_field(lines, "mode") == wanted:
            return
        click(report, fd, MODE_BUTTON, settle_s)
    raise RuntimeError(f"mode {wanted:02X} was not reached")


def capture(args):
    fd, port = open_uart(args.port, args.baud)
    report = [
        "# Current physical rotary mode/range capture",
        f"firmware={args.firmware}",
        f"port={port}",
        f"baud={args.baud}",
        f"time={time.strftime('%Y-%m-%d %H:%M:%S')}",
        "",
    ]
    try:
        read_lines(fd, 0.35)
        send(report, fd, "UDEBUG OFF")
        send(report, fd, "UXCLR")
        app = read_app(report, fd)
        rotary = parse_field(app, "rot")
        if rotary != args.rotary:
            raise RuntimeError(
                f"physical rotary is {rotary!s}, expected {args.rotary:02X}"
            )

        modes = ROTARY_MODES.get(args.rotary)
        if modes is None:
            raise RuntimeError(f"rotary {args.rotary:02X} is not described")

        # Reach a deterministic mode through the same MODE path used by the UI.
        normalize_mode(report, fd, modes[0], args.settle)

        for index, expected_mode in enumerate(modes):
            if not wait_for_mode(report, fd, expected_mode):
                raise RuntimeError(f"expected mode {expected_mode:02X} was not reached")
            time.sleep(args.settle)
            snapshot(report, fd, f"mode={expected_mode:02X} default")

            for manual_index in range(MANUAL_RANGE_COUNTS[expected_mode]):
                click(report, fd, RANGE_BUTTON, args.settle)
                snapshot(
                    report,
                    fd,
                    f"mode={expected_mode:02X} manual-step={manual_index + 1}",
                )

            if index + 1 < len(modes):
                click(report, fd, MODE_BUTTON, args.settle)

        # A complete MODE cycle returns to the deterministic first mode. For
        # single-mode positions this also verifies that MODE is a no-op.
        click(report, fd, MODE_BUTTON, args.settle)
        if all(MANUAL_RANGE_COUNTS[mode] == 0 for mode in modes):
            click(report, fd, RANGE_BUTTON, args.settle)
            snapshot(report, fd, "range-button-no-op")
        send(report, fd, "UXBTN 00")
        send(report, fd, "UXCLR")
        snapshot(report, fd, "final-restored")
    finally:
        close_uart(fd)

    os.makedirs(os.path.dirname(args.output), exist_ok=True)
    with open(args.output, "w", encoding="utf-8") as handle:
        handle.write("\n".join(report))
        handle.write("\n")
    return args.output


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="/dev/cu.usbserial-110")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--rotary", type=lambda value: int(value, 0), default=0x04)
    parser.add_argument("--settle", type=float, default=0.45)
    parser.add_argument("--firmware", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    print(capture(args))


if __name__ == "__main__":
    main()
