#!/usr/bin/env python3
import argparse
import os
import sys
import time

from capture_rotary_modes import close_uart, command, open_uart, read_lines


def read_mode(fd):
    for line in command(fd, "UAPP", delay=0.02, read_time=0.12):
        if not line.startswith("UAPP "):
            continue
        for field in line.split():
            if field.startswith("mode="):
                return int(field[5:], 16)
    return None


def click_mode(fd):
    command(fd, "UXBTN 01", delay=0.03, read_time=0.08)
    command(fd, "UXBTN 00", delay=0.08, read_time=0.12)


def enter_mode(fd, wanted_mode, rows):
    for step in range(6):
        mode = read_mode(fd)
        rows.append(f"# seek_mode_step={step} mode={mode}")
        if mode == wanted_mode:
            return
        click_mode(fd)
        time.sleep(0.3)
    raise RuntimeError(f"could not reach mode 0x{wanted_mode:02X}")


def prepare_cap_then_enter_ohms(fd, rows):
    rows.append("# normalize one complete physical MODE cycle")
    for step in range(4):
        click_mode(fd)
        time.sleep(0.3)
        rows.append(f"# normalize_step={step + 1} mode={read_mode(fd)}")

    for step in range(4):
        mode = read_mode(fd)
        rows.append(f"# seek_cap_step={step} mode={mode}")
        if mode == 0x0C:
            break
        click_mode(fd)
        time.sleep(0.3)
    else:
        raise RuntimeError("could not reach CAP mode before Ohm capture")

    time.sleep(0.5)
    rows.append("# event=MODE_CAP_TO_OHMS")
    click_mode(fd)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Capture compact 121GW Ohm diagnostics during a physical transition."
    )
    parser.add_argument("--port", default="/dev/cu.usbserial-110")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--seconds", type=float, default=30.0)
    parser.add_argument("--interval", type=float, default=0.1)
    parser.add_argument(
        "--read-time",
        type=float,
        default=0.045,
        help="UART response window per sample; keep below --interval",
    )
    parser.add_argument("--command", default="UOHMD")
    parser.add_argument("--prefix", default="UOHMD ")
    parser.add_argument(
        "--enter-ohms",
        action="store_true",
        help="normalize all P10 modes, stop on CAP, then click once into Ohm before capture",
    )
    parser.add_argument(
        "--enter-mode",
        type=lambda value: int(value, 0),
        help="reach this P10 mode using simulated MODE button clicks before capture",
    )
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    fd, actual_port = open_uart(args.port, args.baud)
    rows = [
        "# Ohm physical transition capture",
        f"# port={actual_port} baud={args.baud}",
        f"# interval_s={args.interval:.3f} duration_s={args.seconds:.3f}",
        f"# read_time_s={args.read_time:.3f}",
        f"# wall_time={time.strftime('%Y-%m-%d %H:%M:%S')}",
    ]

    try:
        read_lines(fd, 0.2)
        command(fd, "UDEBUG OFF", delay=0.02, read_time=0.1, flush_before=True)
        if args.enter_ohms:
            prepare_cap_then_enter_ohms(fd, rows)
        elif args.enter_mode is not None:
            enter_mode(fd, args.enter_mode, rows)

        start = time.monotonic()
        next_sample = start
        while True:
            now = time.monotonic()
            elapsed = now - start
            if elapsed >= args.seconds:
                break
            if now < next_sample:
                time.sleep(next_sample - now)

            sample_at = time.monotonic()
            lines = command(
                fd,
                args.command,
                delay=0.0,
                read_time=args.read_time,
                flush_before=False,
            )
            for line in lines:
                if line.startswith(args.prefix):
                    row = f"@@ {sample_at - start:09.3f} {line}"
                    rows.append(row)
                    print(row, flush=True)
            next_sample += args.interval
            if next_sample < time.monotonic():
                next_sample = time.monotonic()
    finally:
        close_uart(fd)

    output_dir = os.path.dirname(args.output)
    if output_dir:
        os.makedirs(output_dir, exist_ok=True)
    with open(args.output, "w", encoding="ascii") as log_file:
        log_file.write("\n".join(rows))
        log_file.write("\n")

    print(args.output)
    return 0


if __name__ == "__main__":
    sys.exit(main())
