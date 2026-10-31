#!/usr/bin/env python3
import os
import sys
import time

from capture_rotary_modes import (
    close_uart,
    command,
    env_cmds,
    open_uart,
    read_lines,
)


P10_ROTARY = 0x10
P10_MODES = [
    ("ohms", 0x09),
    ("continuity", 0x0A),
    ("diode", 0x0B),
    ("cap", 0x0C),
]


def extract_field(line, key):
    marker = key + "="
    if marker not in line:
        return None
    start = line.index(marker) + len(marker)
    return line[start:start + 2]


def app_state(fd):
    lines = command(fd, "UAPP", delay=0.05, read_time=0.18)
    rotary = None
    mode = None
    for line in lines:
        if line.startswith("UAPP "):
            rotary = extract_field(line, "rot")
            mode = extract_field(line, "mode")
    return rotary, mode, lines


def click_mode(fd):
    command(fd, "UXBTN 01", delay=0.05, read_time=0.12)
    command(fd, "UXBTN 00", delay=0.10, read_time=0.18)


def wait_for_p10(fd, timeout_s):
    end = time.time() + timeout_s
    last_lines = []
    while time.time() < end:
        rotary, mode, lines = app_state(fd)
        last_lines = lines
        if rotary == f"{P10_ROTARY:02X}":
            return mode, lines
        time.sleep(0.10)
    return None, last_lines


def drive_to_mode(fd, wanted_mode, report):
    for step in range(6):
        rotary, mode, lines = app_state(fd)
        report.extend(f"<< {line}" for line in lines)
        if rotary != f"{P10_ROTARY:02X}":
            report.append(f"## not-in-p10 rot={rotary} wanted=10")
            return False
        if mode == f"{wanted_mode:02X}":
            return True
        report.append(f">> MODE step={step} wanted={wanted_mode:02X}")
        click_mode(fd)
    return False


def snapshot(fd, label, mode):
    cmds = env_cmds("EEV_P10_SNAPSHOT_COMMANDS", "UTASK,UAPP,UP10D,UFECFG,UFE,ULCD")
    rows = [f"## snapshot {label}"]
    for cmd in cmds:
        rows.append(f">> {cmd}")
        for line in command(fd, cmd, delay=0.04, read_time=0.28, flush_before=(cmd == cmds[0])):
            rows.append(f"<< {line}")
    if mode == 0x0C:
        rows.append(">> UCAPDUMP")
        for line in command(fd, "UCAPDUMP", delay=0.04, read_time=0.35):
            rows.append(f"<< {line}")
    return rows


def capture_window(fd, label, mode, seconds, interval):
    rows = [f"## window {label} seconds={seconds:.2f} interval={interval:.2f}"]
    end = time.time() + seconds
    idx = 0
    while time.time() < end:
        stamp = time.strftime("%H:%M:%S")
        rows.append(f"@@ sample={idx:03d} time={stamp}")
        rows.extend(snapshot(fd, f"{label} sample={idx}", mode))
        idx += 1
        sleep_for = min(interval, max(0.0, end - time.time()))
        if sleep_for > 0:
            time.sleep(sleep_for)
    return rows


def run():
    port = os.environ.get("EEV_UART_PORT", "")
    baud = int(os.environ.get("EEV_UART_BAUD", "19200"))
    seconds = float(os.environ.get("EEV_P10_CAPTURE_SECONDS", "10"))
    interval = float(os.environ.get("EEV_P10_CAPTURE_INTERVAL", "0.5"))
    out_path = os.environ.get("EEV_P10_CAPTURE_OUT", "logs/p10_physical_capture.txt")

    fd, actual_port = open_uart(port, baud)
    report = [
        "# P10 physical rotary capture",
        f"port={actual_port} baud={baud}",
        f"time={time.strftime('%Y-%m-%d %H:%M:%S')}",
        "note=rotary must be physically set to Vohm/diode/continuity/cap",
        "",
    ]
    try:
        read_lines(fd, 0.3)
        command(fd, "UDEBUG OFF", flush_before=True)
        command(fd, "UXCLR", flush_before=True)
        mode, lines = wait_for_p10(fd, 8.0)
        report.extend(f"<< {line}" for line in lines)
        if mode is None:
            report.append("## timeout waiting for physical P10 rotary")
            return 2, report, out_path

        report.append("## normalize one complete MODE cycle")
        for _ in range(4):
            click_mode(fd)
            time.sleep(0.30)

        for name, mode_value in P10_MODES:
            report.append(f"# {name} mode={mode_value:02X}")
            ok = drive_to_mode(fd, mode_value, report)
            report.append(f"## drive ok={1 if ok else 0}")
            report.extend(capture_window(fd, name, mode_value, seconds, interval))
            report.append("")
    finally:
        close_uart(fd)

    return 0, report, out_path


def main():
    rc, report, out_path = run()
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
        f.write("\n")
    print(out_path)
    return rc


if __name__ == "__main__":
    sys.exit(main())
