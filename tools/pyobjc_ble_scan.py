#!/usr/bin/env python3
import sys

import objc
from Foundation import NSDate, NSObject, NSRunLoop
from CoreBluetooth import (
    CBAdvertisementDataLocalNameKey,
    CBAdvertisementDataServiceUUIDsKey,
    CBCentralManager,
    CBCentralManagerScanOptionAllowDuplicatesKey,
)


class Scanner(NSObject):
    central = objc.ivar()

    def init(self):
        self = objc.super(Scanner, self).init()
        if self is None:
            return None
        self.central = CBCentralManager.alloc().initWithDelegate_queue_(self, None)
        return self

    def centralManagerDidUpdateState_(self, central):
        print(f"STATE {int(central.state())}", flush=True)
        if int(central.state()) == 5:
            central.scanForPeripheralsWithServices_options_(
                None, {CBCentralManagerScanOptionAllowDuplicatesKey: True}
            )

    def centralManager_didDiscoverPeripheral_advertisementData_RSSI_(
        self, central, peripheral, advertisement_data, rssi
    ):
        name = peripheral.name() or advertisement_data.get(CBAdvertisementDataLocalNameKey, "") or ""
        services = advertisement_data.get(CBAdvertisementDataServiceUUIDsKey, []) or []
        service_text = ",".join(str(uuid.UUIDString()) for uuid in services)
        print(
            f"DEV id={peripheral.identifier().UUIDString()} name={name!r} "
            f"rssi={rssi} services={service_text}",
            flush=True,
        )


def main():
    timeout = float(sys.argv[1]) if len(sys.argv) > 1 else 8.0
    scanner = Scanner.alloc().init()
    deadline = NSDate.dateWithTimeIntervalSinceNow_(timeout)
    while NSDate.date().compare_(deadline) < 0:
        NSRunLoop.currentRunLoop().runUntilDate_(NSDate.dateWithTimeIntervalSinceNow_(0.1))
    scanner.central.stopScan()
    print("DONE", flush=True)


if __name__ == "__main__":
    main()
