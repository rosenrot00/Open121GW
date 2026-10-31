#!/usr/bin/env python3
import glob
import os
import select
import sys
import termios
import time


ROTARIES = [
    ("rot01_lowz", 0x01),
    ("rot02_v", 0x02),
    ("rot04_mv_temp", 0x04),
    ("rot08_hz", 0x08),
    ("rot10_ohm_cont_diode_cap", 0x10),
    ("rot20_ua_mva", 0x20),
    ("rot40_ma", 0x40),
    ("rot80_a", 0x80),
]

ROTARY_MODE_SEQUENCES = {
    0x01: [0x00],
    0x02: [0x02],
    0x04: [0x05, 0x03, 0x04],
    0x08: [0x06, 0x07, 0x08],
    0x10: [0x09, 0x0A, 0x0B, 0x0C],
    0x20: [0x18, 0x0F],
    0x40: [0x11, 0x10, 0x16, 0x0D],
    0x80: [0x13, 0x12],
}

ROTARY_CYCLE_STEPS = {
    0x01: 1,
    # Stock rot02 is AC -> AC+DC -> DC. AC and AC+DC both report mode 02;
    # the alt-bank state is visible in UP10D/frontend/LCD, not in UAPP.mode.
    0x02: 3,
    0x04: 3,
    0x08: 3,
    0x10: 4,
    0x20: 2,
    0x40: 4,
    0x80: 2,
}

DEFAULT_MODE_RANGES = {mode: 0x00 for modes in ROTARY_MODE_SEQUENCES.values() for mode in modes}
DEFAULT_MODE_RANGES[0x09] = 0x06

BAUDS = {
    19200: termios.B19200,
    115200: termios.B115200,
    230400: getattr(termios, "B230400", termios.B115200),
}

PENDING_BY_FD = {}
SERIAL_BY_FD = {}


def env_cmds(name, default):
    value = os.environ.get(name, default)
    return tuple(cmd.strip() for cmd in value.split(",") if cmd.strip())


def env_bool(name, default):
    value = os.environ.get(name)
    if value is None:
        return default
    return value.strip().lower() not in ("0", "false", "no", "off")


def open_uart(port, baud):
    if not port:
        ports = sorted(glob.glob("/dev/cu.usbserial-*"))
        if not ports:
            raise FileNotFoundError("no /dev/cu.usbserial-* port found")
        port = ports[0]
    try:
        import serial
        ser = serial.Serial(port, baud, timeout=0.05)
        fd = ser.fileno()
        SERIAL_BY_FD[fd] = ser
        return fd, port
    except ImportError:
        from ble_uart_updater import TermiosSerial
        ser = TermiosSerial(port, baud, timeout=0.05)
        SERIAL_BY_FD[ser.fd] = ser
        return ser.fd, port
    fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    attrs = termios.tcgetattr(fd)
    attrs[0] = 0
    attrs[1] = 0
    attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
    attrs[3] = 0
    attrs[4] = BAUDS[baud]
    attrs[5] = BAUDS[baud]
    termios.tcsetattr(fd, termios.TCSANOW, attrs)
    termios.tcflush(fd, termios.TCIOFLUSH)
    return fd, port


def close_uart(fd):
    ser = SERIAL_BY_FD.pop(fd, None)
    if ser is not None:
        if hasattr(ser, "close"):
            ser.close()
        else:
            os.close(ser.fd)
    else:
        os.close(fd)


def uart_write_line(fd, text):
    data = (text + "\n").encode("ascii")
    ser = SERIAL_BY_FD.get(fd)
    if ser is not None:
        ser.write(data)
        ser.flush()
    else:
        os.write(fd, data)
        termios.tcdrain(fd)


def read_lines(fd, seconds):
    end = time.time() + seconds
    pending = PENDING_BY_FD.pop(fd, b"")
    lines = []
    while time.time() < end:
        ser = SERIAL_BY_FD.get(fd)
        if ser is not None:
            old_timeout = ser.timeout
            ser.timeout = min(0.05, max(0.0, end - time.time()))
            data = ser.readline()
            ser.timeout = old_timeout
        else:
            ready, _, _ = select.select([fd], [], [], max(0.0, end - time.time()))
            if not ready:
                break
            try:
                data = os.read(fd, 4096)
            except BlockingIOError:
                continue
        if not data:
            continue
        pending += data
        while b"\n" in pending:
            line, pending = pending.split(b"\n", 1)
            lines.append(line.rstrip(b"\r").decode("ascii", "replace"))
    PENDING_BY_FD[fd] = pending
    return lines


def drain_uart(fd, quiet_s=0.06, max_s=0.6):
    end = time.time() + max_s
    quiet_until = time.time() + quiet_s
    pending = PENDING_BY_FD.pop(fd, b"")
    drained = []
    while time.time() < end and time.time() < quiet_until:
        ser = SERIAL_BY_FD.get(fd)
        if ser is not None:
            old_timeout = ser.timeout
            ser.timeout = min(0.03, max(0.0, quiet_until - time.time()))
            if hasattr(ser, "in_waiting"):
                waiting = ser.in_waiting
                data = ser.read(waiting) if waiting > 0 else b""
            else:
                data = ser.readline()
            ser.timeout = old_timeout
            if not data:
                continue
        else:
            ready, _, _ = select.select([fd], [], [], max(0.0, quiet_until - time.time()))
            if not ready:
                break
            try:
                data = os.read(fd, 4096)
            except BlockingIOError:
                continue
        if not data:
            continue
        quiet_until = time.time() + quiet_s
        pending += data
        while b"\n" in pending:
            line, pending = pending.split(b"\n", 1)
            drained.append(line.rstrip(b"\r").decode("ascii", "replace"))
    PENDING_BY_FD[fd] = pending
    return drained


def command(fd, text, delay=0.12, read_time=0.35, flush_before=False):
    if flush_before:
        drain_uart(fd)
    uart_write_line(fd, text)
    time.sleep(delay)
    return read_lines(fd, read_time)


def snapshot(fd, tag, stock):
    rows = [f"## {tag}"]
    cmds = env_cmds("EEV_CAPTURE_SNAPSHOT_COMMANDS", "UAPP,UP10D,UFECFG,UFE,ULCD")
    for cmd in cmds:
        rows.append(f">> {cmd}")
        read_time = 0.40 if cmd == "UFECFG" else 0.25
        for line in command(fd, cmd, delay=0.08, read_time=read_time):
            rows.append(f"<< {line}")
    return rows


def capture_window(fd, tag, seconds, interval_s):
    rows = [f"## {tag} window={seconds:.1f}s interval={interval_s:.3f}s"]
    enable_debug = env_bool("EEV_CAPTURE_ENABLE_DEBUG", True)
    cmds = env_cmds("EEV_CAPTURE_WINDOW_COMMANDS", "UAPP,UP10D,UFECFG,UFE,ULCD")
    if enable_debug:
        command(fd, "UDBG 1", delay=0.05, read_time=0.15)
    start = time.time()
    next_poll = start
    while time.time() - start < seconds:
        now = time.time()
        if now >= next_poll:
            stamp_ms = int((now - start) * 1000)
            for cmd in cmds:
                rows.append(f"@@ {stamp_ms:05d} >> {cmd}")
                read_time = 0.42 if cmd == "UFECFG" else 0.28
                for line in command(fd, cmd, delay=0.02, read_time=read_time):
                    rows.append(f"@@ {int((time.time() - start) * 1000):05d} << {line}")
            next_poll = now + interval_s
        else:
            time.sleep(0.01)
    if enable_debug:
        command(fd, "UDBG 0", delay=0.05, read_time=0.15)
    return rows


def capture_transition(fd, tag, seconds, interval_s):
    rows = [f"## {tag} transition={seconds:.3f}s interval={interval_s:.3f}s"]
    start = time.time()
    next_poll = start
    pending = PENDING_BY_FD.pop(fd, b"")
    cmds = env_cmds("EEV_CAPTURE_TRANSITION_COMMANDS", "UAPP,UP10S")
    while time.time() - start < seconds:
        now = time.time()
        if now >= next_poll:
            stamp_ms = int((now - start) * 1000)
            for cmd in cmds:
                rows.append(f"@@ {stamp_ms:05d} >> {cmd}")
                uart_write_line(fd, cmd)
            next_poll = now + interval_s

        ser = SERIAL_BY_FD.get(fd)
        if ser is not None:
            old_timeout = ser.timeout
            ser.timeout = 0.01
            if hasattr(ser, "in_waiting"):
                waiting = ser.in_waiting
                data = ser.read(waiting) if waiting > 0 else b""
            else:
                data = ser.readline()
            ser.timeout = old_timeout
            if not data:
                continue
        else:
            ready, _, _ = select.select([fd], [], [], 0.01)
            if not ready:
                continue
            try:
                data = os.read(fd, 4096)
            except BlockingIOError:
                continue
        if not data:
            continue
        pending += data
        while b"\n" in pending:
            line, pending = pending.split(b"\n", 1)
            stamp_ms = int((time.time() - start) * 1000)
            text = line.rstrip(b"\r").decode("ascii", "replace")
            rows.append(f"@@ {stamp_ms:05d} << {text}")
    drain_deadline = time.time() + 1.0
    quiet_deadline = time.time() + 0.15
    while time.time() < drain_deadline and time.time() < quiet_deadline:
        ser = SERIAL_BY_FD.get(fd)
        if ser is not None:
            old_timeout = ser.timeout
            ser.timeout = min(0.03, max(0.0, quiet_deadline - time.time()))
            if hasattr(ser, "in_waiting"):
                waiting = ser.in_waiting
                data = ser.read(waiting) if waiting > 0 else b""
            else:
                data = ser.readline()
            ser.timeout = old_timeout
            if not data:
                continue
        else:
            ready, _, _ = select.select([fd], [], [], max(0.0, quiet_deadline - time.time()))
            if not ready:
                break
            try:
                data = os.read(fd, 4096)
            except BlockingIOError:
                continue
        if not data:
            continue
        quiet_deadline = time.time() + 0.15
        pending += data
        while b"\n" in pending:
            line, pending = pending.split(b"\n", 1)
            stamp_ms = int((time.time() - start) * 1000)
            text = line.rstrip(b"\r").decode("ascii", "replace")
            rows.append(f"@@ {stamp_ms:05d} << {text}")
    PENDING_BY_FD[fd] = pending
    return rows


def click_mode(fd, stock):
    command(fd, "UXBTN 01", delay=0.06, read_time=0.15)
    command(fd, "UXBTN 00", delay=0.12, read_time=0.25)


def set_mode_range(fd, mode, range_value):
    rows = []
    ok = False
    for attempt in range(3):
        lines = command(fd, f"UP10SET {mode:02X} {range_value:02X}",
                        delay=0.16, read_time=0.70, flush_before=True)
        # Custom firmware returns UOK plus a diagnostic UP10D snapshot, while
        # the stock patch returns UP10SETOK. Do not store the diagnostic UP10D
        # here; otherwise the compare parser attributes it to the previous mode.
        rows.extend(line for line in lines if not line.startswith("UP10D "))
        if any(line.startswith("UP10SETOK") or line == "UOK" for line in lines):
            ok = True
            break
        rows.append(f"## UP10SET retry={attempt + 1}")
    if not ok:
        rows.append("## UP10SET no-ack")
    return rows


def extract_mode(lines):
    for line in lines:
        if "mode=" in line:
            start = line.index("mode=") + 5
            try:
                return int(line[start:start + 2], 16)
            except ValueError:
                return None
    return None


def current_mode(fd, stock):
    return extract_mode(command(fd, "UAPP", delay=0.05, read_time=0.15))


def wait_for_mode(fd, mode, timeout_s=1.5):
    end = time.time() + timeout_s
    last = []
    wanted = f"mode={mode:02X}"
    while time.time() < end:
        last = command(fd, "UAPP", delay=0.08, read_time=0.22)
        if any(wanted in line for line in last):
            return True, last
    return False, last


def wait_for_rotary(fd, rotary, timeout_s=1.5):
    end = time.time() + timeout_s
    last = []
    while time.time() < end:
        last = command(fd, "UAPP", delay=0.05, read_time=0.15)
        wanted = f"rot={rotary:02X}"
        if any(wanted in line for line in last):
            return True, last
    return False, last


def selected_rotaries():
    wanted = os.environ.get("EEV_CAPTURE_ROTARIES", "").strip()
    if not wanted:
        return ROTARIES
    names = {item.strip() for item in wanted.split(",") if item.strip()}
    return [(name, rotary) for name, rotary in ROTARIES
            if name in names or f"{rotary:02X}" in names or f"0x{rotary:02X}" in names]


def capture(port="", baud=19200, out_path="logs/rotary_modes_capture.txt", stock=False,
            window_seconds=10.0, interval_s=0.5, transition_seconds=0.3,
            transition_interval_s=0.05, temp_tenths_c=None, battery_percent=None,
            battery_low=False, direct_modes=False):
    fd, actual_port = open_uart(port, baud)
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    report = [
        f"# rotary/mode capture",
        f"port={actual_port} baud={baud}",
        f"time={time.strftime('%Y-%m-%d %H:%M:%S')}",
        "",
    ]
    try:
        read_lines(fd, 0.4)
        command(fd, "UDEBUG OFF", flush_before=True)
        command(fd, "UXJACK 00", flush_before=True)
        touched_temp_override = False
        touched_battery_override = False
        if temp_tenths_c is not None:
            temp_word = temp_tenths_c & 0xFFFF
            report.append(f">> UTOVR {temp_word:04X}")
            for line in command(fd, f"UTOVR {temp_word:04X}",
                                delay=0.08, read_time=0.25, flush_before=True):
                report.append(f"<< {line}")
            report.append(">> UTUNIT 01")
            for line in command(fd, "UTUNIT 01", delay=0.08, read_time=0.25, flush_before=True):
                report.append(f"<< {line}")
            touched_temp_override = True
        if battery_percent is not None:
            pct = max(0, min(99, battery_percent))
            low = 1 if battery_low else 0
            report.append(f">> UBOVR {pct:02X} {low:02X}")
            for line in command(fd, f"UBOVR {pct:02X} {low:02X}",
                                delay=0.08, read_time=0.25, flush_before=True):
                report.append(f"<< {line}")
            touched_battery_override = True
        for name, rotary in selected_rotaries():
            report.append(f"# {name} rot={rotary:02X}")
            seen = []
            mode_sequence = ROTARY_MODE_SEQUENCES.get(rotary, []) if direct_modes else []
            step_count = len(mode_sequence) if direct_modes else ROTARY_CYCLE_STEPS.get(rotary, 10)
            if not direct_modes:
                report.append(f">> UXROT {rotary:02X}")
                for _ in range(3):
                    lines = command(fd, f"UXROT {rotary:02X}", delay=0.05,
                                    read_time=0.15, flush_before=True)
                    report.extend(f"<< {line}" for line in lines)
                    ok, last = wait_for_rotary(fd, rotary)
                    report.extend(f"<< {line}" for line in last)
                    if ok:
                        break
                report.extend(capture_transition(fd, f"{name} after-rotary rot={rotary:02X}",
                                                 transition_seconds, transition_interval_s))
            for idx in range(step_count):
                if direct_modes:
                    requested_mode = mode_sequence[idx]
                    requested_range = DEFAULT_MODE_RANGES.get(requested_mode, 0x00)
                    report.append(f">> UP10SET {requested_mode:02X} {requested_range:02X}")
                    for line in set_mode_range(fd, requested_mode, requested_range):
                        report.append(f"<< {line}")
                    ok, last = wait_for_mode(fd, requested_mode)
                    report.extend(f"<< {line}" for line in last)
                    if not ok:
                        report.append(f"## {name} direct-mode-timeout wanted={requested_mode:02X}")
                    report.extend(capture_transition(fd, f"{name} after-direct-mode step={idx}",
                                                     transition_seconds, transition_interval_s))
                mode = current_mode(fd, stock)
                if mode is None:
                    report.append(f"## {name} step={idx} mode=??")
                else:
                    if not direct_modes and mode in seen and rotary != 0x02:
                        report.append(f"## {name} cycle-repeat mode={mode:02X}")
                        break
                    seen.append(mode)
                    report.extend(snapshot(fd, f"{name} step={idx} mode={mode:02X}", stock))
                    report.extend(capture_window(fd, f"{name} step={idx} mode={mode:02X}",
                                                 window_seconds, interval_s))
                if not direct_modes:
                    click_mode(fd, stock)
                    report.extend(capture_transition(fd, f"{name} after-mode-click step={idx}",
                                                     transition_seconds, transition_interval_s))
            report.append("")
        if touched_temp_override:
            command(fd, "UTOVR FFFFFFFF")
            command(fd, "UTUNIT FF")
        if touched_battery_override:
            command(fd, "UBOVR FF")
        command(fd, "UXCLR")
    finally:
        close_uart(fd)
    with open(out_path, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
        f.write("\n")
    return out_path


if __name__ == "__main__":
    port = os.environ.get("EEV_UART_PORT", "")
    baud = int(os.environ.get("EEV_UART_BAUD", "19200"))
    stock = os.environ.get("EEV_CAPTURE_STOCK", "0") == "1"
    window_seconds = float(os.environ.get("EEV_CAPTURE_SECONDS", "10"))
    interval_s = float(os.environ.get("EEV_CAPTURE_INTERVAL", "0.5"))
    transition_seconds = float(os.environ.get("EEV_CAPTURE_TRANSITION_SECONDS", "0.3"))
    transition_interval_s = float(os.environ.get("EEV_CAPTURE_TRANSITION_INTERVAL", "0.1"))
    temp_env = os.environ.get("EEV_CAPTURE_TEMP_TENTHS_C", "").strip()
    temp_tenths_c = int(temp_env, 0) if temp_env else None
    battery_env = os.environ.get("EEV_CAPTURE_BATTERY_PERCENT", "").strip()
    battery_percent = int(battery_env, 0) if battery_env else None
    battery_low = os.environ.get("EEV_CAPTURE_BATTERY_LOW", "0").strip() in ("1", "true", "TRUE", "yes", "YES")
    direct_modes = os.environ.get("EEV_CAPTURE_DIRECT_MODES", "0") == "1"
    out_path = sys.argv[1] if len(sys.argv) > 1 else "logs/rotary_modes_capture.txt"
    print(capture(port=port, baud=baud, out_path=out_path, stock=stock,
                  window_seconds=window_seconds, interval_s=interval_s,
                  transition_seconds=transition_seconds,
                  transition_interval_s=transition_interval_s,
                  temp_tenths_c=temp_tenths_c,
                  battery_percent=battery_percent,
                  battery_low=battery_low,
                  direct_modes=direct_modes))
