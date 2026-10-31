import CoreBluetooth
import Foundation

final class BLEDump: NSObject, CBCentralManagerDelegate, CBPeripheralDelegate {
    private var central: CBCentralManager!
    private var target: CBPeripheral?
    private var writeChar: CBCharacteristic?
    private var rxLine = ""
    private let logPath = "/tmp/eev_ble_dump.log"
    private let targetName = "121GW"
    private let start: UInt32
    private let length: Int
    private let outputPath: String
    private var offset = 0
    private var bytes = Data()
    private var pendingAddr: UInt32?
    private var pendingEndOffset: Int?
    private var requestSerial = 0
    private var retryCount = 0
    private let preflightCommands = ["UABORT\n", "USTOPDBG\n"]
    private var preflightIndex = 0

    override init() {
        let args = Array(ProcessInfo.processInfo.arguments.dropFirst())
        start = UInt32(args.count > 0 ? args[0] : "08000000", radix: 16) ?? 0x0800_0000
        length = Int(args.count > 1 ? args[1] : "6000", radix: 16) ?? 0x6000
        outputPath = args.count > 2 ? args[2] : "/tmp/eevblog121gw_bootloader.bin"
        super.init()
        try? "BLE dump start start=\(String(format: "%08X", start)) len=\(String(format: "%X", length)) out=\(outputPath)\n"
            .write(toFile: logPath, atomically: true, encoding: .utf8)
        central = CBCentralManager(delegate: self, queue: DispatchQueue.main)
        DispatchQueue.main.asyncAfter(deadline: .now() + 1800.0) {
            self.log("TIMEOUT")
            exit(3)
        }
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
            log("CHAR \(ch.uuid.uuidString) props=\(ch.properties.rawValue)")
            if ch.properties.contains(.notify) || ch.properties.contains(.indicate) {
                peripheral.setNotifyValue(true, for: ch)
            }
            if ch.properties.contains(.write) || ch.properties.contains(.writeWithoutResponse) {
                writeChar = ch
            }
        }
        DispatchQueue.main.asyncAfter(deadline: .now() + 1.0) {
            self.sendPreflight()
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
        if let error = error {
            log("WRITE_ERR \(error.localizedDescription)")
            exit(6)
        }
    }

    private func parseHexBytes(_ text: String) -> Data? {
        guard text.count % 2 == 0 else { return nil }
        var data = Data()
        var i = text.startIndex
        while i < text.endIndex {
            let j = text.index(i, offsetBy: 2)
            guard let value = UInt8(text[i..<j], radix: 16) else { return nil }
            data.append(value)
            i = j
        }
        return data
    }

    private func leadingHex(_ text: String, count: Int) -> String? {
        var out = ""
        for ch in text {
            if ch.isHexDigit {
                out.append(ch)
                if out.count == count {
                    return out
                }
            } else {
                break
            }
        }
        return nil
    }

    private func handleLine(_ line: String) {
        if preflightIndex < preflightCommands.count {
            if line.hasPrefix("UOK") || line.hasPrefix("UDBG") {
                sendPreflight()
                return
            }
            if line.hasPrefix("UERR") {
                log("FAIL \(line)")
                exit(7)
            }
            return
        }
        guard line.hasPrefix("URD ") else {
            if line.hasPrefix("UERR") {
                log("FAIL \(line)")
                exit(7)
            }
            return
        }
        let parts = line.split(separator: " ", maxSplits: 3).map(String.init)
        guard parts.count == 4,
              let addr = UInt32(parts[1], radix: 16),
              let count = Int(parts[2], radix: 16),
              let hex = leadingHex(parts[3], count: count * 2),
              let chunk = parseHexBytes(hex),
              chunk.count == count else {
            log("RETRY malformed \(line)")
            retryOrFail()
            return
        }
        let expectedAddr = start + UInt32(offset)
        guard addr == expectedAddr else {
            if addr < expectedAddr {
                log("SKIP stale got=\(String(format: "%08X", addr)) expected=\(String(format: "%08X", expectedAddr))")
                return
            }
            log("RETRY addr got=\(String(format: "%08X", addr)) expected=\(String(format: "%08X", expectedAddr))")
            retryOrFail()
            return
        }
        retryCount = 0
        bytes.append(chunk)
        offset += chunk.count
        if offset % 1024 == 0 || offset == length {
            try? bytes.write(to: URL(fileURLWithPath: outputPath), options: .atomic)
            log("PROGRESS \(String(format: "%X", offset))/\(String(format: "%X", length))")
        }
        if let pendingEnd = pendingEndOffset, offset < pendingEnd {
            return
        }
        pendingAddr = nil
        pendingEndOffset = nil
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.02) {
            self.sendNext()
        }
    }

    private func sendPreflight() {
        guard preflightIndex < preflightCommands.count else {
            sendNext()
            return
        }
        guard let peripheral = target, let ch = writeChar else {
            log("ERROR no writable characteristic")
            exit(10)
        }
        let line = preflightCommands[preflightIndex]
        preflightIndex += 1
        log("TX \(line.trimmingCharacters(in: .whitespacesAndNewlines))")
        peripheral.writeValue(Data(line.utf8), for: ch, type: .withResponse)
        DispatchQueue.main.asyncAfter(deadline: .now() + 1.0) {
            if self.preflightIndex <= self.preflightCommands.count {
                self.sendPreflight()
            }
        }
    }

    private func retryOrFail() {
        guard retryCount < 5 else {
            log("FAIL retry limit at \(String(format: "%08X", start + UInt32(offset)))")
            exit(8)
        }
        retryCount += 1
        pendingAddr = nil
        pendingEndOffset = nil
        DispatchQueue.main.asyncAfter(deadline: .now() + 2.0) {
            self.sendNext()
        }
    }

    private func sendNext() {
        guard let peripheral = target, let ch = writeChar else {
            log("ERROR no writable characteristic")
            exit(10)
        }
        if offset >= length {
            do {
                try bytes.write(to: URL(fileURLWithPath: outputPath), options: .atomic)
                log("DONE bytes=\(bytes.count) out=\(outputPath)")
                exit(0)
            } catch {
                log("ERROR write \(error.localizedDescription)")
                exit(11)
            }
        }
        let remaining = length - offset
        let count = min(8, remaining)
        let addr = start + UInt32(offset)
        let line = String(format: "UR %08X %X\n", addr, count)
        pendingAddr = addr
        pendingEndOffset = offset + count
        requestSerial += 1
        let serial = requestSerial
        log("TX \(line.trimmingCharacters(in: .whitespacesAndNewlines))")
        peripheral.writeValue(Data(line.utf8), for: ch, type: .withResponse)
        DispatchQueue.main.asyncAfter(deadline: .now() + 5.0) {
            if self.requestSerial == serial, let pending = self.pendingAddr {
                self.log("FAIL no URD for \(String(format: "%08X", pending))")
                exit(12)
            }
        }
    }
}

let dumper = BLEDump()
RunLoop.main.run()
_ = dumper
