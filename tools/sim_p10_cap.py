#!/usr/bin/env python3
import os
import glob
import select
import termios
import time


PORT = os.environ.get("EEV_UART_PORT", "")
BAUD = termios.B19200


def open_uart():
    port = PORT
    if not port:
        matches = sorted(glob.glob("/dev/cu.usbserial-*"))
        if not matches:
            raise FileNotFoundError("no /dev/cu.usbserial-* port found")
        port = matches[0]
    print("PORT", port)
    fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    attrs = termios.tcgetattr(fd)
    attrs[0] = 0
    attrs[1] = 0
    attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
    attrs[3] = 0
    attrs[4] = BAUD
    attrs[5] = BAUD
    termios.tcsetattr(fd, termios.TCSANOW, attrs)
    termios.tcflush(fd, termios.TCIOFLUSH)
    return fd


def read_lines(fd, seconds):
    end = time.time() + seconds
    pending = b""
    lines = []
    while time.time() < end:
        ready, _, _ = select.select([fd], [], [], max(0.0, end - time.time()))
        if not ready:
            break
        try:
            data = os.read(fd, 1024)
        except BlockingIOError:
            continue
        if not data:
            continue
        pending += data
        while b"\n" in pending:
            line, pending = pending.split(b"\n", 1)
            lines.append(line.rstrip(b"\r").decode("ascii", "replace"))
    return lines


def command(fd, text, delay=0.2, read_time=0.35):
    os.write(fd, (text + "\n").encode("ascii"))
    termios.tcdrain(fd)
    time.sleep(delay)
    lines = read_lines(fd, read_time)
    print(">>", text)
    for line in lines:
        print("<<", line)
    return lines


def click_mode(fd):
    command(fd, "UXBTN 01", delay=0.08, read_time=0.25)
    time.sleep(0.08)
    command(fd, "UXBTN 00", delay=0.12, read_time=0.3)
    time.sleep(0.18)


def current_p10_mode(fd):
    lines = command(fd, "UP10D", delay=0.08, read_time=0.25)
    for line in lines:
        marker = "mode="
        if marker in line:
            pos = line.index(marker) + len(marker)
            try:
                return int(line[pos:pos + 2], 16)
            except ValueError:
                pass
    return None


def enter_cap_mode(fd):
    for _ in range(6):
        mode = current_p10_mode(fd)
        if mode == 0x0C:
            return True
        click_mode(fd)
    return current_p10_mode(fd) == 0x0C


def main():
    fd = open_uart()
    try:
        read_lines(fd, 0.5)
        command(fd, "UDEBUG OFF")
        command(fd, "UXCLR")
        command(fd, "UXROT 10", delay=0.3)
        time.sleep(0.25)
        if not enter_cap_mode(fd):
            print("failed to enter CAP mode")
            return
        command(fd, "UXIN")
        for delay in (0.2, 0.8, 2.0, 4.0):
            time.sleep(delay)
            command(fd, "UP10D")
            command(fd, "UFE")
            command(fd, "UGPIO")
            command(fd, "ULCD")
    finally:
        os.close(fd)


if __name__ == "__main__":
    main()
