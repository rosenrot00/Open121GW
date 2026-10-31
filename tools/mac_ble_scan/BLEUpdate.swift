import CoreBluetooth
import Foundation

struct UpdateCommand {
    let label: String
    let line: String
    let expected: String
}

final class BLEUpdate: NSObject, CBCentralManagerDelegate, CBPeripheralDelegate {
    private var central: CBCentralManager!
    private var target: CBPeripheral?
    private var writeChar: CBCharacteristic?
    private var commands = [UpdateCommand]()
    private var nextCommand = 0
    private var currentExpected = ""
    private var rxLine = ""
    private let logPath = "/tmp/eev_ble_update.log"
    private let targetName = "121GW"

    override init() {
        super.init()
        commands = Self.makeCommands()
        try? "BLE update start commands=\(commands.count)\n".write(toFile: logPath, atomically: true, encoding: .utf8)
        central = CBCentralManager(delegate: self, queue: DispatchQueue.main)
        DispatchQueue.main.asyncAfter(deadline: .now() + 600.0) {
            self.log("TIMEOUT")
            exit(3)
        }
    }

    private static func crc32(_ data: Data) -> UInt32 {
        var crc: UInt32 = 0xFFFF_FFFF
        for byte in data {
            crc ^= UInt32(byte)
            for _ in 0..<8 {
                if (crc & 1) != 0 {
                    crc = (crc >> 1) ^ 0xEDB8_8320
                } else {
                    crc >>= 1
                }
            }
        }
        return crc ^ 0xFFFF_FFFF
    }

    private static func hex(_ data: Data) -> String {
        data.map { String(format: "%02X", $0) }.joined()
    }

    private static func makeCommands() -> [UpdateCommand] {
        let args = ProcessInfo.processInfo.arguments.dropFirst()
        let data: Data
        if let path = args.first, let fileData = try? Data(contentsOf: URL(fileURLWithPath: path)) {
            data = fileData
        } else {
            data = Data([0x00, 0x20, 0x00, 0x20, 0x01, 0x61, 0x00, 0x08])
        }

        let crc = crc32(data)
        var out = [UpdateCommand]()
        out.append(UpdateCommand(label: "UB",
                                 line: String(format: "UB %X %08X\n", data.count, crc),
                                 expected: "UOK"))
        var offset = 0
        while offset < data.count {
            let end = min(offset + 32, data.count)
            let chunk = data.subdata(in: offset..<end)
            out.append(UpdateCommand(label: "UD",
                                     line: "UD \(hex(chunk))\n",
                                     expected: "UACK"))
            offset = end
        }
        out.append(UpdateCommand(label: "UE", line: "UE\n", expected: "UOK SAFE_NOFLASH"))
        return out
    }

    private func log(_ text: String) {
        let line = text + "\n"
        guard let data = line.data(using: .utf8) else { return }
        if FileManager.default.fileExists(atPath: logPath),
           let handle = try? FileHandle(forWritingTo: URL(fileURLWithPath: logPath)) {
            try? handle.seekToEnd()
            try? handle.write(contentsOf: data)
            try? handle.close()
        } else {
            try? data.write(to: URL(fileURLWithPath: logPath))
        }
    }

    private func props(_ p: CBCharacteristicProperties) -> String {
        var out = [String]()
        if p.contains(.writeWithoutResponse) { out.append("writeWithoutResponse") }
        if p.contains(.write) { out.append("write") }
        if p.contains(.notify) { out.append("notify") }
        if p.contains(.indicate) { out.append("indicate") }
        return out.joined(separator: ",")
    }

    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        log("STATE \(central.state.rawValue)")
        guard central.state == .poweredOn else { return }
        central.scanForPeripherals(withServices: nil,
                                   options: [CBCentralManagerScanOptionAllowDuplicatesKey: true])
    }

    func centralManager(_ central: CBCentralManager,
                        didDiscover peripheral: CBPeripheral,
                        advertisementData: [String: Any],
                        rssi RSSI: NSNumber) {
        let name = peripheral.name ?? advertisementData[CBAdvertisementDataLocalNameKey] as? String ?? ""
        if name == targetName {
            log("CONNECT id=\(peripheral.identifier.uuidString) rssi=\(RSSI)")
            target = peripheral
            peripheral.delegate = self
            central.stopScan()
            central.connect(peripheral, options: nil)
        }
    }

    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        log("CONNECTED")
        peripheral.discoverServices(nil)
    }

    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        if let error = error {
            log("ERROR services \(error.localizedDescription)")
            exit(4)
        }
        for service in peripheral.services ?? [] {
            log("SERVICE \(service.uuid.uuidString)")
            peripheral.discoverCharacteristics(nil, for: service)
        }
    }

    func peripheral(_ peripheral: CBPeripheral,
                    didDiscoverCharacteristicsFor service: CBService,
                    error: Error?) {
        if let error = error {
            log("ERROR chars \(error.localizedDescription)")
            exit(5)
        }
        for ch in service.characteristics ?? [] {
            log("CHAR \(ch.uuid.uuidString) props=\(props(ch.properties))")
            if ch.properties.contains(.notify) || ch.properties.contains(.indicate) {
                peripheral.setNotifyValue(true, for: ch)
            }
            if ch.properties.contains(.write) || ch.properties.contains(.writeWithoutResponse) {
                writeChar = ch
            }
        }
        DispatchQueue.main.asyncAfter(deadline: .now() + 1.0) {
            self.sendNext()
        }
    }

    func peripheral(_ peripheral: CBPeripheral,
                    didUpdateNotificationStateFor characteristic: CBCharacteristic,
                    error: Error?) {
        log("NOTIFY \(characteristic.uuid.uuidString) enabled=\(characteristic.isNotifying) err=\(error?.localizedDescription ?? "")")
    }

    func peripheral(_ peripheral: CBPeripheral,
                    didUpdateValueFor characteristic: CBCharacteristic,
                    error: Error?) {
        if let error = error {
            log("RXERR \(error.localizedDescription)")
            return
        }
        for byte in characteristic.value ?? Data() {
            if byte == 0x0D || byte == 0x0A {
                handleLine(rxLine)
                rxLine = ""
            } else if byte >= 0x20 && byte <= 0x7E {
                rxLine.append(Character(UnicodeScalar(byte)))
            }
        }
    }

    func peripheral(_ peripheral: CBPeripheral,
                    didWriteValueFor characteristic: CBCharacteristic,
                    error: Error?) {
        log("WRITE_DONE err=\(error?.localizedDescription ?? "")")
    }

    private func handleLine(_ line: String) {
        guard !line.isEmpty else { return }
        log("RX \(line)")
        if line.hasPrefix("UERR") {
            log("FAIL \(line)")
            exit(6)
        }
        if !currentExpected.isEmpty && line.contains(currentExpected) {
            currentExpected = ""
            DispatchQueue.main.asyncAfter(deadline: .now() + 0.02) {
                self.sendNext()
            }
        }
    }

    private func sendNext() {
        guard let peripheral = target, let ch = writeChar else {
            log("ERROR no writable characteristic")
            exit(7)
        }
        if nextCommand >= commands.count {
            log("DONE")
            exit(0)
        }
        let cmd = commands[nextCommand]
        nextCommand += 1
        currentExpected = cmd.expected
        log("TX \(cmd.label) \(cmd.line.trimmingCharacters(in: .whitespacesAndNewlines))")
        peripheral.writeValue(Data(cmd.line.utf8), for: ch, type: .withResponse)
    }
}

let updater = BLEUpdate()
RunLoop.main.run()
_ = updater
