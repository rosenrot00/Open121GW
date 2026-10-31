#!/usr/bin/env python3
import argparse
import os
import re
import time

from capture_rotary_modes import close_uart, command, open_uart, read_lines


BAR_SEGMENTS = (
    (2, 0x00000002), (4, 0x00000002), (6, 0x00000002),
    (6, 0x00000004), (4, 0x00000004), (2, 0x00000004),
    (0, 0x00000004), (0, 0x00000008), (2, 0x00000008),
    (4, 0x00000008), (6, 0x00000008), (6, 0x00000010),
    (4, 0x00000010), (2, 0x00000010), (0, 0x00000010),
    (0, 0x00000020), (2, 0x00000020), (4, 0x00000020),
    (6, 0x00000020), (6, 0x00000040), (4, 0x00000040),
    (2, 0x00000040), (0, 0x00000040), (0, 0x00000080),
    (2, 0x00000080),
)


def parse_lcd(lines):
    for line in lines:
        if line.startswith("ULCD "):
            parts = line.split()[1:]
            if len(parts) == 16:
                return [int(part, 16) for part in parts]
    return None


def parse_diag(lines):
    for line in lines:
        if line.startswith("UP10D "):
            return dict(re.findall(r"([A-Za-z0-9_]+)=([0-9A-Fa-f]+)", line))
    return {}


def bar_count(words):
    count = 0
    for word, bit in BAR_SEGMENTS:
        if (words[word] & bit) == 0:
            break
        count += 1
    return count


def capture(args):
    fd, port = open_uart(args.port, args.baud)
    rows = [
        "# LCD bargraph capture",
        f"firmware={args.firmware} port={port} baud={args.baud}",
        f"interval={args.interval:.3f}s duration={args.duration:.3f}s",
    ]
    counts = set()
    try:
        read_lines(fd, 0.25)
        start = time.monotonic()
        sample = 0
        diag = {}
        while time.monotonic() - start < args.duration:
            target = start + sample * args.interval
            delay = target - time.monotonic()
            if delay > 0:
                time.sleep(delay)
            if sample % args.diag_every == 0:
                diag = parse_diag(command(fd, "UP10D", delay=0.005, read_time=0.055))
            words = parse_lcd(command(fd, "ULCD", delay=0.005, read_time=0.055))
            elapsed_ms = int((time.monotonic() - start) * 1000)
            if words is None:
                rows.append(f"{elapsed_ms:05d} lcd=missing")
            else:
                count = bar_count(words)
                counts.add(count)
                rows.append(
                    f"{elapsed_ms:05d} bars={count:02d} "
                    f"mode={diag.get('mode', '--')} range={diag.get('range', '--')} "
                    f"r1f={diag.get('r1f', '--')} r20={diag.get('r20', '--')} "
                    f"sfl={diag.get('sfl', '--')} inst={diag.get('inst', '--------')} "
                    f"val={diag.get('val', '--------')} "
                    f"lcd0={words[0]:08X} lcd2={words[2]:08X} "
                    f"lcd4={words[4]:08X} lcd6={words[6]:08X}"
                )
            sample += 1
    finally:
        close_uart(fd)

    rows.append("counts=" + ",".join(str(count) for count in sorted(counts)))
    os.makedirs(os.path.dirname(args.output), exist_ok=True)
    with open(args.output, "w", encoding="utf-8") as handle:
        handle.write("\n".join(rows))
        handle.write("\n")
    print(rows[-1])
    print(args.output)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="/dev/cu.usbserial-110")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--duration", type=float, default=10.0)
    parser.add_argument("--interval", type=float, default=0.1)
    parser.add_argument("--diag-every", type=int, default=5)
    parser.add_argument("--firmware", required=True)
    parser.add_argument("--output", required=True)
    capture(parser.parse_args())


if __name__ == "__main__":
    main()
