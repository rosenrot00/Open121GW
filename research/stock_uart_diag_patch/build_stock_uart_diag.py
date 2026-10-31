#!/usr/bin/env python3
import hashlib
import os
import shutil
import struct
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parent
IDA_PRO = ROOT.parent
ORIGINAL = Path("/Users/taarimi7/Downloads/EEVBlog2_05.bin")
OUT = IDA_PRO / "EEVBlog2_05_stock_uart_capdiag_updater.bin"
BUILD = ROOT / "build"

APP_BASE = 0x08006000
APP_END = 0x08060000
PAYLOAD_ADDR = 0x080294E8

HOOKS = {
    0x0800AA78: "hook_cap_compute",
    0x08006148: "hook_cap_timeout",
    0x0800C528: "hook_bt_init_stock_ble_low",
    0x0800C52E: "hook_bt_disable_keep_uart_ble_low",
    0x08014988: "hook_rotary_sample_override",
    0x0801C608: "hook_bt_disable_keep_uart_ble_low",
    0x08024830: "hook_usart_read_data",
    0x08024B4E: "hook_board_init_uart_ready_ble_low",
    0x08024D56: "hook_bt_process_rx_start_updater",
}

WORD_HOOKS = {}


def cross_gcc() -> str:
    found = shutil.which("arm-none-eabi-gcc")
    if found:
        return found
    candidates = [
        "/Applications/ModusToolbox/tools_3.0/gcc/bin/arm-none-eabi-gcc",
        "/Applications/STM32CubeIDE.app/Contents/Eclipse/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.12.3.rel1.macos64_1.1.0.202410301201/tools/bin/arm-none-eabi-gcc",
    ]
    for candidate in candidates:
        if Path(candidate).exists():
            return candidate
    raise SystemExit("arm-none-eabi-gcc not found")


def run(cmd):
    print("+", " ".join(str(c) for c in cmd))
    subprocess.run(cmd, check=True, cwd=ROOT)


def read_cstr(data: bytes, off: int) -> str:
    end = data.find(b"\0", off)
    if end < 0:
        end = len(data)
    return data[off:end].decode("utf-8", errors="replace")


def parse_elf_symbols(data: bytes) -> dict[str, int]:
    if data[:4] != b"\x7fELF" or data[4] != 1 or data[5] != 1:
        raise SystemExit("expected 32-bit little-endian ELF")

    e_shoff = struct.unpack_from("<I", data, 32)[0]
    e_shentsize = struct.unpack_from("<H", data, 46)[0]
    e_shnum = struct.unpack_from("<H", data, 48)[0]

    sections = []
    for idx in range(e_shnum):
        off = e_shoff + idx * e_shentsize
        sh = struct.unpack_from("<IIIIIIIIII", data, off)
        sections.append(
            {
                "name": sh[0],
                "type": sh[1],
                "addr": sh[3],
                "offset": sh[4],
                "size": sh[5],
                "link": sh[6],
                "entsize": sh[9],
            }
        )

    out = {}
    for section in sections:
        if section["type"] != 2:
            continue
        strtab = sections[section["link"]]
        str_data = data[strtab["offset"] : strtab["offset"] + strtab["size"]]
        entsize = section["entsize"] or 16
        for off in range(section["offset"], section["offset"] + section["size"], entsize):
            st_name, st_value, _st_size, st_info, _st_other, _st_shndx = struct.unpack_from("<IIIBBH", data, off)
            if st_name == 0:
                continue
            name = read_cstr(str_data, st_name)
            typ = st_info & 0x0F
            if typ in (1, 2):
                out[name] = st_value
    return out


def elf_flash_image(data: bytes) -> tuple[int, bytes]:
    e_phoff = struct.unpack_from("<I", data, 28)[0]
    e_phentsize = struct.unpack_from("<H", data, 42)[0]
    e_phnum = struct.unpack_from("<H", data, 44)[0]
    segments = []
    for idx in range(e_phnum):
        off = e_phoff + idx * e_phentsize
        p_type, p_offset, _p_vaddr, p_paddr, p_filesz, _p_memsz, _p_flags, _p_align = struct.unpack_from(
            "<IIIIIIII", data, off
        )
        if p_type != 1 or p_filesz == 0:
            continue
        if APP_BASE <= p_paddr < APP_END:
            segments.append((p_paddr, p_offset, p_filesz))

    if not segments:
        raise SystemExit("payload ELF has no flash-loadable segments")

    base = min(addr for addr, _off, _size in segments)
    end = max(addr + size for addr, _off, size in segments)
    image = bytearray([0xFF]) * (end - base)
    for addr, off, size in segments:
        image[addr - base : addr - base + size] = data[off : off + size]
    return base, bytes(image)


def encode_thumb_bl(src: int, target: int) -> bytes:
    target &= ~1
    imm = target - (src + 4)
    if imm & 1:
        raise ValueError("unaligned BL target")
    if not (-(1 << 24) <= imm < (1 << 24)):
        raise ValueError(f"BL target out of range: {src:08X}->{target:08X}")
    imm25 = imm & 0x01FFFFFF
    s = (imm25 >> 24) & 1
    i1 = (imm25 >> 23) & 1
    i2 = (imm25 >> 22) & 1
    imm10 = (imm25 >> 12) & 0x03FF
    imm11 = (imm25 >> 1) & 0x07FF
    j1 = (~(i1 ^ s)) & 1
    j2 = (~(i2 ^ s)) & 1
    hw1 = 0xF000 | (s << 10) | imm10
    hw2 = 0xF800 | (j1 << 13) | (j2 << 11) | imm11
    return struct.pack("<HH", hw1, hw2)


def build_payload() -> tuple[dict[str, int], int, bytes]:
    BUILD.mkdir(exist_ok=True)
    gcc = cross_gcc()
    obj = BUILD / "stock_uart_diag_payload.o"
    elf = BUILD / "stock_uart_diag_payload.elf"
    map_file = BUILD / "stock_uart_diag_payload.map"
    run(
        [
            gcc,
            "-mcpu=cortex-m3",
            "-mthumb",
            "-std=c11",
            "-ffreestanding",
            "-fno-builtin",
            "-fdata-sections",
            "-ffunction-sections",
            "-fno-unwind-tables",
            "-fno-asynchronous-unwind-tables",
            "-Os",
            "-Wall",
            "-Wextra",
            "-c",
            "stock_uart_diag_payload.c",
            "-o",
            str(obj),
        ]
    )
    run(
        [
            gcc,
            "-mcpu=cortex-m3",
            "-mthumb",
            "-nostdlib",
            "-Wl,-n",
            "-Wl,-T,payload.ld",
            "-Wl,--gc-sections",
            f"-Wl,-Map,{map_file}",
            str(obj),
            "-o",
            str(elf),
        ]
    )
    data = elf.read_bytes()
    symbols = parse_elf_symbols(data)
    base, image = elf_flash_image(data)
    return symbols, base, image


def main() -> int:
    if not ORIGINAL.exists():
        raise SystemExit(f"missing original firmware: {ORIGINAL}")
    original = ORIGINAL.read_bytes()
    if APP_BASE + len(original) != PAYLOAD_ADDR:
        raise SystemExit(f"unexpected original size {len(original):#x}; payload address would not match")

    symbols, payload_base, payload = build_payload()
    if payload_base != PAYLOAD_ADDR:
        raise SystemExit(f"unexpected payload base {payload_base:#x}")
    if PAYLOAD_ADDR + len(payload) > APP_END:
        raise SystemExit("payload exceeds protected app flash range")

    image = bytearray(original)
    image.extend(payload)

    for hook_addr, sym in HOOKS.items():
        if sym not in symbols:
            raise SystemExit(f"missing symbol {sym}")
        off = hook_addr - APP_BASE
        old = image[off : off + 4]
        if len(old) != 4 or old[1] & 0xF8 != 0xF0:
            raise SystemExit(f"hook at {hook_addr:#x} does not look like Thumb BL: {old.hex()}")
        image[off : off + 4] = encode_thumb_bl(hook_addr, symbols[sym])
        print(f"patched {hook_addr:#010x}: {old.hex()} -> {image[off:off+4].hex()} -> {sym}@{symbols[sym]:#010x}")

    for hook_addr, sym in WORD_HOOKS.items():
        if sym not in symbols:
            raise SystemExit(f"missing symbol {sym}")
        off = hook_addr - APP_BASE
        old = image[off : off + 4]
        value = symbols[sym] | 1
        image[off : off + 4] = struct.pack("<I", value)
        print(f"patched word {hook_addr:#010x}: {old.hex()} -> {image[off:off+4].hex()} -> {sym}@{value:#010x}")

    OUT.write_bytes(image)
    print(f"wrote {OUT}")
    print(f"size {len(image)} / {len(image):#x}")
    print(f"sha256 {hashlib.sha256(image).hexdigest()}")

    if os.environ.get("EEVBLOG_COPY_TO_SD") == "1":
        for vol in (Path("/Volumes/NO NAME"), Path("/Volumes/UNTITLED")):
            if vol.exists():
                dst = vol / "EEVBlog.bin"
                shutil.copyfile(OUT, dst)
                print(f"copied {dst}")
                subprocess.run(["sync"], check=False)
                subprocess.run(["diskutil", "eject", str(vol)], check=False)
                break

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
