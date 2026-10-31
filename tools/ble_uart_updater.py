#!/usr/bin/env python3
import argparse
import asyncio
import binascii
import os
import select
import sys
import termios
import time


def chunk_hex(data, size):
    for offset in range(0, len(data), size):
        yield data[offset:offset + size]


def make_update_lines(path, chunk_size, flash=False, flash_sim=False, data_baud=None):
    data = open(path, "rb").read()
    crc = binascii.crc32(data) & 0xFFFFFFFF
    baud_arg = f" {data_baud:X}" if data_baud else ""
    if flash_sim:
        yield "USIM", f"USIM {len(data):X} {crc:08X}{baud_arg}\n"
    elif flash:
        yield "UFBEGIN", f"UFBEGIN {len(data):X} {crc:08X}{baud_arg}\n"
    else:
        yield "UB", f"UB {len(data):X} {crc:08X}{baud_arg}\n"
    for chunk in chunk_hex(data, chunk_size):
        yield "UD", "UD " + chunk.hex().upper() + "\n"
    if flash_sim:
        yield "UFSIM", "UE\n"
    elif flash:
        yield "UF", "UE\n"
    else:
        yield "UE", "UE\n"


def expected_reply(tag):
    if tag == "UCLK":
        return ("UCLK",)
    if tag == "UB":
        return ("UOK",)
    if tag == "USIM":
        return ("URAM SIM",)
    if tag == "UFBEGIN":
        return ("URAM FLASH", "RAM FLASH")
    if tag == "UD":
        return ("UACK",)
    if tag == "UE":
        return ("UOK SAFE_NOFLASH",)
    if tag == "UF":
        return ("UFLASHOK",)
    if tag == "UFSIM":
        return ("UFSIMOK",)
    return (
        "UOK", "UERR", "UINFO", "USTATE", "UDBG", "UHELLO", "SCAP",
        "UCLK", "UAPP", "ULCD", "ULCDR", "ULCDT", "UP10", "UFECFG",
        "UFE", "UGPIO", "UX", "UT", "UBAT", "UBOVR", "USD", "UCAP", "UOHM",
        "UTASK", "UCONT", "C,",
    )


def accept_reply(tag, text):
    if text.startswith("URX "):
        return False
    if text.startswith("UERR"):
        raise RuntimeError(text)
    return any(text.startswith(prefix) for prefix in expected_reply(tag))


def run_serial(args):
    def open_serial(baud):
        try:
            import serial
            return serial.Serial(args.port, baud, timeout=0.05)
        except ImportError:
            return TermiosSerial(args.port, baud, timeout=0.05)

    ser = open_serial(args.baud)

    def reopen_serial(baud):
        nonlocal ser
        try:
            ser.close()
        except AttributeError:
            os.close(ser.fd)
        except Exception:
            pass
        time.sleep(0.08)
        ser = open_serial(baud)

    def read_expected(tag, deadline):
        while time.time() < deadline:
            data = ser.readline()
            if data:
                text = data.decode("ascii", errors="replace").rstrip()
                print("<<", text)
                if accept_reply(tag, text):
                    return
        raise TimeoutError(f"No matching response to {tag}")

    def send_line(tag, line):
        print(">>", line.rstrip())
        ser.write(line.encode("ascii"))
        ser.flush()
        read_expected(tag, time.time() + args.reply_timeout)

    def send_first_update_line(tag, line):
        print(">>", line.rstrip())
        ser.write(line.encode("ascii"))
        ser.flush()
        if args.data_baud == args.baud:
            read_expected(tag, time.time() + args.reply_timeout)
            return

        deadline = time.time() + args.reply_timeout
        saw_start = False
        saw_switch = False
        while time.time() < deadline:
            data = ser.readline()
            if data:
                text = data.decode("ascii", errors="replace").rstrip()
                print("<<", text)
                if text.startswith("USWITCH"):
                    saw_switch = True
                    break
                if text.startswith("UPDSTART"):
                    saw_start = True
                    continue
                if accept_reply(tag, text):
                    return
        if not saw_switch and not saw_start:
            print("~~ no UPDSTART/USWITCH at entry baud; trying data baud")
        print(f"~~ baud {args.baud} -> {args.data_baud}")
        reopen_serial(args.data_baud)
        read_expected(tag, time.time() + args.reply_timeout)

    try:
        if args.image:
            first = True
            data_baud = args.data_baud if args.data_baud != args.baud else None
            for tag, line in make_update_lines(args.image, args.chunk_size, args.flash, args.flash_sim, data_baud):
                if first:
                    send_first_update_line(tag, line)
                    first = False
                else:
                    send_line(tag, line)
        else:
            send_line("CMD", args.command.rstrip("\r\n") + "\n")
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1
    return 0


class TermiosSerial:
    BAUDS = {
        9600: termios.B9600,
        19200: termios.B19200,
        38400: termios.B38400,
        57600: termios.B57600,
        115200: termios.B115200,
        230400: getattr(termios, "B230400", 230400),
        460800: getattr(termios, "B460800", 460800),
        921600: getattr(termios, "B921600", 921600),
    }

    def __init__(self, port, baud, timeout=0.05):
        if baud not in self.BAUDS:
            raise ValueError(f"unsupported baud without pyserial: {baud}")
        self.fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        self.timeout = timeout
        attrs = termios.tcgetattr(self.fd)
        attrs[0] = 0
        attrs[1] = 0
        attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
        attrs[3] = 0
        attrs[4] = self.BAUDS[baud]
        attrs[5] = self.BAUDS[baud]
        termios.tcsetattr(self.fd, termios.TCSANOW, attrs)
        termios.tcflush(self.fd, termios.TCIOFLUSH)

    def write(self, data):
        return os.write(self.fd, data)

    def flush(self):
        termios.tcdrain(self.fd)

    def readline(self):
        end = time.time() + self.timeout
        out = bytearray()
        while time.time() < end:
            r, _, _ = select.select([self.fd], [], [], max(0.0, end - time.time()))
            if not r:
                break
            try:
                chunk = os.read(self.fd, 1)
            except BlockingIOError:
                continue
            if not chunk:
                continue
            out.extend(chunk)
            if chunk == b"\n":
                break
            end = time.time() + self.timeout
        return bytes(out)


async def find_ble_device(name_filter, address):
    from bleak import BleakScanner

    if address:
        return address

    devices = await BleakScanner.discover(timeout=8.0)
    for dev in devices:
        print(f"SCAN {dev.name!r} {dev.address}")
    for dev in devices:
        if dev.name and name_filter.lower() in dev.name.lower():
            return dev.address
    return None


async def run_ble_async(args):
    try:
        from bleak import BleakClient
    except ImportError:
        print("bleak is missing: python3 -m pip install bleak", file=sys.stderr)
        return 2

    address = await find_ble_device(args.name, args.address)
    if not address:
        print("Kein passendes BLE-Geraet gefunden", file=sys.stderr)
        return 1

    notify_started = 0
    writable = []
    pending = []

    def on_notify(sender, data):
        text = data.decode("ascii", errors="replace").rstrip()
        print(f"<< [{sender}] {text if text else data.hex(' ')}")
        pending.append(text)

    async with BleakClient(address) as client:
        print("CONNECTED", address)
        for service in client.services:
            print("SERVICE", service.uuid)
            for char in service.characteristics:
                props = ",".join(char.properties)
                print("  CHAR", char.uuid, props)
                if "notify" in char.properties or "indicate" in char.properties:
                    try:
                        await client.start_notify(char.uuid, on_notify)
                        notify_started += 1
                    except Exception as exc:
                        print("  notify failed", char.uuid, exc)
                if "write" in char.properties or "write-without-response" in char.properties:
                    writable.append(char)

        if not writable:
            print("No writable characteristic found", file=sys.stderr)
            return 1
        print(f"NOTIFY {notify_started}, WRITE {len(writable)}")

        write_char = writable[0]
        response = "write" in write_char.properties

        async def send_line(tag, line):
            print(">>", line.rstrip())
            await client.write_gatt_char(write_char.uuid, line.encode("ascii"), response=response)
            deadline = time.monotonic() + args.reply_timeout
            while time.monotonic() < deadline:
                while pending:
                    text = pending.pop(0)
                    if accept_reply(tag, text):
                        return
                await asyncio.sleep(0.02)
            raise TimeoutError(f"No matching response to {tag}")

        try:
            if args.image:
                for tag, line in make_update_lines(args.image, args.chunk_size, args.flash, args.flash_sim):
                    await send_line(tag, line)
            else:
                await send_line("CMD", args.command.rstrip("\r\n") + "\n")
        except Exception as exc:
            print(f"ERROR: {exc}", file=sys.stderr)
            return 1

        await asyncio.sleep(1.0)
    return 0


def run_ble(args):
    return asyncio.run(run_ble_async(args))


def main():
    parser = argparse.ArgumentParser(description="EEVBlog 121GW BLE/UART updater transport test")
    parser.add_argument("--image", help="Firmware image to stream in safe no-flash mode")
    parser.add_argument("--flash-sim", action="store_true", help="Run the SRAM flash routine in simulation mode after CRC/vector validation")
    parser.add_argument("--flash", action="store_true", help="After CRC/vector validation, write the image to app flash and reset")
    parser.add_argument("--command", default="U?", help="Single command to send when --image is omitted")
    parser.add_argument("--chunk-size", type=int, default=16)
    parser.add_argument("--reply-timeout", type=float, default=2.0)
    sub = parser.add_subparsers(dest="transport", required=True)

    ble = sub.add_parser("ble")
    ble.add_argument("--name", default="121GW")
    ble.add_argument("--address")
    ble.set_defaults(func=run_ble)

    serial_p = sub.add_parser("serial")
    serial_p.add_argument("port")
    serial_p.add_argument("--baud", type=int, default=19200, help="Baudrate used to enter the SRAM updater")
    serial_p.add_argument("--data-baud", type=int, default=19200, help="Baudrate used after UPDSTART inside the SRAM updater")
    serial_p.set_defaults(func=run_serial)

    args = parser.parse_args()
    if args.flash and args.flash_sim:
        parser.error("--flash and --flash-sim are mutually exclusive")
    raise SystemExit(args.func(args))


if __name__ == "__main__":
    main()
