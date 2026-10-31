#!/usr/bin/env python3
import argparse
import sys
import time


def main() -> int:
    parser = argparse.ArgumentParser(description="Probe the 121GW UART pin-test firmware.")
    parser.add_argument("port", help="Serial device, for example /dev/tty.usbserial-XXXX")
    parser.add_argument("--baud", type=int, default=19200)
    parser.add_argument("--interval", type=float, default=1.0)
    args = parser.parse_args()

    try:
        import serial
    except ImportError:
        print("pyserial is missing. Install it with: python3 -m pip install pyserial", file=sys.stderr)
        return 2

    with serial.Serial(args.port, args.baud, timeout=0.2) as ser:
        print(f"opened {args.port} at {args.baud} 8N1")
        next_probe = 0.0
        while True:
            now = time.monotonic()
            if now >= next_probe:
                ser.write(b"?")
                ser.flush()
                next_probe = now + args.interval

            data = ser.read(256)
            if data:
                text = data.decode("ascii", errors="replace")
                print(text, end="", flush=True)


if __name__ == "__main__":
    raise SystemExit(main())
