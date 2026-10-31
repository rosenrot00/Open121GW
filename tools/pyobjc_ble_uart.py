#!/usr/bin/env python3
import argparse
import sys

import objc
from Foundation import NSDate, NSObject, NSRunLoop, NSData
from CoreBluetooth import (
    CBAdvertisementDataLocalNameKey,
    CBCharacteristicWriteWithResponse,
    CBCharacteristicWriteWithoutResponse,
    CBCentralManager,
    CBCentralManagerScanOptionAllowDuplicatesKey,
)


PROP_WRITE = 0x08
PROP_WRITE_NO_RESPONSE = 0x04
PROP_NOTIFY = 0x10
PROP_INDICATE = 0x20


class BLEUart(NSObject):
    central = objc.ivar()
    target_name = objc.ivar()
    target_id = objc.ivar()
    command = objc.ivar()
    peripheral = objc.ivar()
    write_char = objc.ivar()
    connected = objc.ivar()
    sent = objc.ivar()
    done = objc.ivar()
    rx_line = objc.ivar()
    result = objc.ivar()

    def initWithTarget_command_(self, target_name, command):
        self = objc.super(BLEUart, self).init()
        if self is None:
            return None
        self.target_name = target_name
        self.target_id = None
        self.command = command
        self.peripheral = None
        self.write_char = None
        self.connected = False
        self.sent = False
        self.done = False
        self.rx_line = ""
        self.result = 1
        self.central = CBCentralManager.alloc().initWithDelegate_queue_(self, None)
        return self

    def py_log(self, text):
        print(text, flush=True)

    def centralManagerDidUpdateState_(self, central):
        self.py_log(f"STATE {int(central.state())}")
        if int(central.state()) == 5:
            central.scanForPeripheralsWithServices_options_(
                None, {CBCentralManagerScanOptionAllowDuplicatesKey: True}
            )

    def centralManager_didDiscoverPeripheral_advertisementData_RSSI_(
        self, central, peripheral, advertisement_data, rssi
    ):
        name = peripheral.name() or advertisement_data.get(CBAdvertisementDataLocalNameKey, "") or ""
        if name:
            self.py_log(f"DISC id={peripheral.identifier().UUIDString()} name={name!r} rssi={rssi}")
        if name == self.target_name:
            self.py_log(f"CONNECT id={peripheral.identifier().UUIDString()}")
            self.peripheral = peripheral
            peripheral.setDelegate_(self)
            central.stopScan()
            central.connectPeripheral_options_(peripheral, None)

    def centralManager_didConnectPeripheral_(self, central, peripheral):
        self.connected = True
        self.py_log(f"CONNECTED id={peripheral.identifier().UUIDString()}")
        peripheral.discoverServices_(None)

    def centralManager_didFailToConnectPeripheral_error_(self, central, peripheral, error):
        self.py_log(f"FAIL_CONNECT {error}")
        self.done = True
        self.result = 2

    def centralManager_didDisconnectPeripheral_error_(self, central, peripheral, error):
        self.py_log(f"DISCONNECTED {error}")
        if not self.done:
            self.done = True
            self.result = 3

    def peripheral_didDiscoverServices_(self, peripheral, error):
        if error is not None:
            self.py_log(f"ERROR services {error}")
            self.done = True
            self.result = 4
            return
        for service in peripheral.services() or []:
            self.py_log(f"SERVICE {service.UUID().UUIDString()}")
            peripheral.discoverCharacteristics_forService_(None, service)

    def peripheral_didDiscoverCharacteristicsForService_error_(self, peripheral, service, error):
        if error is not None:
            self.py_log(f"ERROR chars {error}")
            self.done = True
            self.result = 5
            return
        for char in service.characteristics() or []:
            props = int(char.properties())
            labels = []
            if props & PROP_WRITE:
                labels.append("write")
            if props & PROP_WRITE_NO_RESPONSE:
                labels.append("writeWithoutResponse")
            if props & PROP_NOTIFY:
                labels.append("notify")
            if props & PROP_INDICATE:
                labels.append("indicate")
            self.py_log(f"CHAR {char.UUID().UUIDString()} props={','.join(labels)}")
            if props & (PROP_NOTIFY | PROP_INDICATE):
                peripheral.setNotifyValue_forCharacteristic_(True, char)
            if self.write_char is None and props & (PROP_WRITE | PROP_WRITE_NO_RESPONSE):
                self.write_char = char
        self.try_send()

    def peripheral_didUpdateNotificationStateForCharacteristic_error_(self, peripheral, char, error):
        self.py_log(
            f"NOTIFY {char.UUID().UUIDString()} enabled={bool(char.isNotifying())} "
            f"err={error if error is not None else ''}"
        )
        self.try_send()

    def peripheral_didUpdateValueForCharacteristic_error_(self, peripheral, char, error):
        if error is not None:
            self.py_log(f"RXERR {error}")
            return
        data = bytes(char.value() or b"")
        hex_text = " ".join(f"{b:02X}" for b in data)
        ascii_text = "".join(chr(b) if 32 <= b <= 126 else "." for b in data)
        self.py_log(f"RX hex={hex_text} ascii={ascii_text!r}")
        for byte in data:
            if byte in (0x0A, 0x0D):
                if self.rx_line:
                    self.handle_line(self.rx_line)
                    self.rx_line = ""
            elif 0x20 <= byte <= 0x7E:
                self.rx_line += chr(byte)

    def peripheral_didWriteValueForCharacteristic_error_(self, peripheral, char, error):
        self.py_log(f"WRITE_DONE err={error if error is not None else ''}")

    def handle_line(self, line):
        self.py_log(f"LINE {line}")
        if line.startswith(("UOK", "UINFO", "USTATE", "UACK", "UERR", "URD")):
            self.done = True
            self.result = 0 if not line.startswith("UERR") else 6

    def try_send(self):
        if self.command is None or self.sent or self.write_char is None or self.peripheral is None:
            return
        props = int(self.write_char.properties())
        write_type = CBCharacteristicWriteWithResponse if props & PROP_WRITE else CBCharacteristicWriteWithoutResponse
        data = NSData.dataWithBytes_length_(self.command.encode("ascii"), len(self.command))
        self.sent = True
        self.py_log(f"TX {self.command.rstrip()}")
        self.peripheral.writeValue_forCharacteristic_type_(data, self.write_char, write_type)


def run_loop_until(client, timeout):
    deadline = NSDate.dateWithTimeIntervalSinceNow_(timeout)
    while not client.done and NSDate.date().compare_(deadline) < 0:
        NSRunLoop.currentRunLoop().runUntilDate_(NSDate.dateWithTimeIntervalSinceNow_(0.05))
    if not client.done:
        print("TIMEOUT", flush=True)
        return 9
    return int(client.result)


def main():
    parser = argparse.ArgumentParser(description="EEVblog 121GW BLE UART via PyObjC/CoreBluetooth")
    parser.add_argument("--name", default="121GW")
    parser.add_argument("--command", default="U?")
    parser.add_argument("--listen", action="store_true")
    parser.add_argument("--timeout", type=float, default=15.0)
    args = parser.parse_args()

    command = None if args.listen else (args.command.rstrip("\r\n") + "\n")
    client = BLEUart.alloc().initWithTarget_command_(args.name, command)
    raise SystemExit(run_loop_until(client, args.timeout))


if __name__ == "__main__":
    main()
