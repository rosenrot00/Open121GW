import CoreBluetooth
import Foundation

final class BLEScanner: NSObject, CBCentralManagerDelegate {
    private var central: CBCentralManager!
    private let logPath = "/tmp/eev_ble_scan.log"
    private var seen = Set<UUID>()

    override init() {
        super.init()
        try? "BLE scan start\n".write(toFile: logPath, atomically: true, encoding: .utf8)
        central = CBCentralManager(delegate: self, queue: DispatchQueue.main)
    }

    private func log(_ text: String) {
        let line = text + "\n"
        if let data = line.data(using: .utf8) {
            if FileManager.default.fileExists(atPath: logPath),
               let handle = try? FileHandle(forWritingTo: URL(fileURLWithPath: logPath)) {
                try? handle.seekToEnd()
                try? handle.write(contentsOf: data)
                try? handle.close()
            } else {
                try? data.write(to: URL(fileURLWithPath: logPath))
            }
        }
    }

    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        log("STATE \(central.state.rawValue)")
        guard central.state == .poweredOn else {
            if central.state == .unauthorized {
                log("ERROR unauthorized")
                exit(2)
            }
            return
        }

        log("SCAN begin")
        central.scanForPeripherals(withServices: nil,
                                   options: [CBCentralManagerScanOptionAllowDuplicatesKey: true])
        DispatchQueue.main.asyncAfter(deadline: .now() + 15.0) {
            central.stopScan()
            self.log("SCAN done unique=\(self.seen.count)")
            exit(0)
        }
    }

    func centralManager(_ central: CBCentralManager,
                        didDiscover peripheral: CBPeripheral,
                        advertisementData: [String: Any],
                        rssi RSSI: NSNumber) {
        seen.insert(peripheral.identifier)
        let name = peripheral.name ?? advertisementData[CBAdvertisementDataLocalNameKey] as? String ?? ""
        let services = (advertisementData[CBAdvertisementDataServiceUUIDsKey] as? [CBUUID])?
            .map { $0.uuidString }
            .joined(separator: ",") ?? ""
        let txPower = advertisementData[CBAdvertisementDataTxPowerLevelKey] as? NSNumber
        log("DEV id=\(peripheral.identifier.uuidString) name='\(name)' rssi=\(RSSI) tx=\(txPower?.stringValue ?? "") services=\(services)")
    }
}

let scanner = BLEScanner()
RunLoop.main.run()
_ = scanner
