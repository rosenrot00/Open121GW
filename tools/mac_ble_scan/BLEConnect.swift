import CoreBluetooth
import Foundation

final class BLEConnect: NSObject, CBCentralManagerDelegate, CBPeripheralDelegate {
    private var central: CBCentralManager!
    private var target: CBPeripheral?
    private var writable = [CBCharacteristic]()
    private let logPath = "/tmp/eev_ble_connect.log"
    private let targetName = "121GW"
    private let sendProbes = ProcessInfo.processInfo.arguments.contains("--probe")
    private let uartPinTest = ProcessInfo.processInfo.arguments.contains("--uart-pin-test")
    private let txLinkTest = ProcessInfo.processInfo.arguments.contains("--tx-link-test")
    private var txSeq: UInt16 = 0
    private var txLinkWaitingForWriteDone = false

    override init() {
        super.init()
        try? "BLE connect start\n".write(toFile: logPath, atomically: true, encoding: .utf8)
        central = CBCentralManager(delegate: self, queue: DispatchQueue.main)
        DispatchQueue.main.asyncAfter(deadline: .now() + 35.0) {
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

    private func props(_ p: CBCharacteristicProperties) -> String {
        var out = [String]()
        if p.contains(.broadcast) { out.append("broadcast") }
        if p.contains(.read) { out.append("read") }
        if p.contains(.writeWithoutResponse) { out.append("writeWithoutResponse") }
        if p.contains(.write) { out.append("write") }
        if p.contains(.notify) { out.append("notify") }
        if p.contains(.indicate) { out.append("indicate") }
        if p.contains(.authenticatedSignedWrites) { out.append("signedWrite") }
        return out.joined(separator: ",")
    }

    private func ascii(_ data: Data) -> String {
        let text = String(data: data, encoding: .utf8) ?? ""
        return text.map { ch in
            if ch == "\r" { return "\\r" }
            if ch == "\n" { return "\\n" }
            if ch.unicodeScalars.allSatisfy({ $0.value >= 0x20 && $0.value <= 0x7E }) {
                return String(ch)
            }
            return "."
        }.joined()
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
        log("SCAN begin target=\(targetName)")
        central.scanForPeripherals(withServices: nil,
                                   options: [CBCentralManagerScanOptionAllowDuplicatesKey: true])
    }

    func centralManager(_ central: CBCentralManager,
                        didDiscover peripheral: CBPeripheral,
                        advertisementData: [String: Any],
                        rssi RSSI: NSNumber) {
        let name = peripheral.name ?? advertisementData[CBAdvertisementDataLocalNameKey] as? String ?? ""
        let services = (advertisementData[CBAdvertisementDataServiceUUIDsKey] as? [CBUUID])?
            .map { $0.uuidString }
            .joined(separator: ",") ?? ""
        log("DISC id=\(peripheral.identifier.uuidString) name='\(name)' rssi=\(RSSI) services=\(services)")
        if name == targetName {
            log("CONNECT id=\(peripheral.identifier.uuidString)")
            target = peripheral
            peripheral.delegate = self
            central.stopScan()
            central.connect(peripheral, options: nil)
        }
    }

    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        log("CONNECTED id=\(peripheral.identifier.uuidString) name='\(peripheral.name ?? "")'")
        peripheral.discoverServices(nil)
    }

    func centralManager(_ central: CBCentralManager,
                        didFailToConnect peripheral: CBPeripheral,
                        error: Error?) {
        log("FAIL_CONNECT \(error?.localizedDescription ?? "")")
        exit(4)
    }

    func centralManager(_ central: CBCentralManager,
                        didDisconnectPeripheral peripheral: CBPeripheral,
                        error: Error?) {
        log("DISCONNECTED \(error?.localizedDescription ?? "")")
        exit(5)
    }

    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        if let error = error {
            log("ERROR services \(error.localizedDescription)")
            return
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
            log("ERROR chars \(service.uuid.uuidString) \(error.localizedDescription)")
            return
        }
        for ch in service.characteristics ?? [] {
            log("CHAR service=\(service.uuid.uuidString) uuid=\(ch.uuid.uuidString) props=\(props(ch.properties))")
            if ch.properties.contains(.notify) || ch.properties.contains(.indicate) {
                peripheral.setNotifyValue(true, for: ch)
            }
            if ch.properties.contains(.write) || ch.properties.contains(.writeWithoutResponse) {
                writable.append(ch)
            }
        }
        if txLinkTest {
            DispatchQueue.main.asyncAfter(deadline: .now() + 1.0) {
                self.sendTxLinkFrame()
            }
        } else if uartPinTest {
            DispatchQueue.main.asyncAfter(deadline: .now() + 1.0) {
                self.sendUartPinProbe()
            }
        } else if sendProbes {
            DispatchQueue.main.asyncAfter(deadline: .now() + 2.0) {
                self.sendProbe("UB 8 942C87B1\n", label: "UB")
            }
            DispatchQueue.main.asyncAfter(deadline: .now() + 5.0) {
                self.sendProbe("UD 0020002001610008\n", label: "UD")
            }
            DispatchQueue.main.asyncAfter(deadline: .now() + 8.0) {
                self.sendProbe("UE\n", label: "UE")
            }
        }
    }

    func peripheral(_ peripheral: CBPeripheral,
                    didUpdateNotificationStateFor characteristic: CBCharacteristic,
                    error: Error?) {
        log("NOTIFY uuid=\(characteristic.uuid.uuidString) enabled=\(characteristic.isNotifying) err=\(error?.localizedDescription ?? "")")
    }

    func peripheral(_ peripheral: CBPeripheral,
                    didUpdateValueFor characteristic: CBCharacteristic,
                    error: Error?) {
        if let error = error {
            log("RXERR uuid=\(characteristic.uuid.uuidString) \(error.localizedDescription)")
            return
        }
        let data = characteristic.value ?? Data()
        log("RX uuid=\(characteristic.uuid.uuidString) hex=\(data.map { String(format: "%02X", $0) }.joined(separator: " ")) ascii='\(ascii(data))'")
    }

    func peripheral(_ peripheral: CBPeripheral,
                    didWriteValueFor characteristic: CBCharacteristic,
                    error: Error?) {
        log("WRITE_DONE uuid=\(characteristic.uuid.uuidString) err=\(error?.localizedDescription ?? "")")
        if txLinkTest {
            txLinkWaitingForWriteDone = false
            sendTxLinkFrame()
        }
    }

    private func sendProbe(_ command: String, label: String) {
        guard let peripheral = target else { return }
        let data = Data(command.utf8)
        log("PROBE writable=\(writable.count)")
        for ch in writable {
            let type: CBCharacteristicWriteType = ch.properties.contains(.write) ? .withResponse : .withoutResponse
            log("WRITE uuid=\(ch.uuid.uuidString) data=\(label)")
            peripheral.writeValue(data, for: ch, type: type)
        }
    }

    private func sendUartPinProbe() {
        guard let peripheral = target else { return }
        let data = Data("?".utf8)
        log("UARTPIN_WRITE writable=\(writable.count) data=?")
        for ch in writable {
            let type: CBCharacteristicWriteType = ch.properties.contains(.write) ? .withResponse : .withoutResponse
            peripheral.writeValue(data, for: ch, type: type)
        }
        DispatchQueue.main.asyncAfter(deadline: .now() + 1.0) {
            self.sendUartPinProbe()
        }
    }

    private func hexDigit(_ value: UInt8) -> UInt8 {
        let nibble = value & 0x0F
        return nibble < 10 ? 0x30 + nibble : 0x41 + nibble - 10
    }

    private func makeTxLinkFrame(_ seq: UInt16) -> Data {
        var bytes = [UInt8](repeating: 0x20, count: 120)
        bytes[0] = 0x55
        bytes[1] = hexDigit(UInt8(seq >> 12))
        bytes[2] = hexDigit(UInt8(seq >> 8))
        bytes[3] = hexDigit(UInt8(seq >> 4))
        bytes[4] = hexDigit(UInt8(seq))
        for i in 0..<111 {
            bytes[5 + i] = 0x61 + UInt8((Int(seq) + i) % 26)
        }
        var xor: UInt8 = 0
        for i in 0..<116 {
            xor ^= bytes[i]
        }
        bytes[116] = hexDigit(xor >> 4)
        bytes[117] = hexDigit(xor)
        bytes[118] = 0x0D
        bytes[119] = 0x0A
        return Data(bytes)
    }

    private func sendTxLinkFrame() {
        guard let peripheral = target else { return }
        if txLinkWaitingForWriteDone {
            return
        }
        let seq = txSeq
        txSeq &+= 1
        let data = makeTxLinkFrame(seq)
        log("TXLINK_WRITE seq=\(seq) hex=\(data.map { String(format: "%02X", $0) }.joined(separator: " "))")
        for ch in writable {
            let type: CBCharacteristicWriteType = ch.properties.contains(.write) ? .withResponse : .withoutResponse
            peripheral.writeValue(data, for: ch, type: type)
            if type == .withResponse {
                txLinkWaitingForWriteDone = true
            }
        }
        if !txLinkWaitingForWriteDone {
            DispatchQueue.main.async {
                self.sendTxLinkFrame()
            }
        }
    }
}

private var client: BLEConnect? = BLEConnect()
RunLoop.main.run()
