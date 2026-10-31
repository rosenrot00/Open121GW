#!/usr/bin/env python3
import re
import sys
from collections import defaultdict


SECTION_RE = re.compile(r"^## (?P<rot>\S+) step=(?P<step>\d+) mode=(?P<mode>[0-9A-F]{2})")
KV_RE = re.compile(r"([A-Za-z0-9_]+)=([0-9A-Fa-f]+)")


def parse_kv(line):
    return {key: value.upper() for key, value in KV_RE.findall(line)}


def lcd_words(line):
    parts = line.split()
    try:
        idx = parts.index("ULCD")
    except ValueError:
        return None
    words = parts[idx + 1:idx + 17]
    return words if len(words) == 16 else None


def parse(path):
    sections = {}
    current = None
    for raw in open(path, encoding="utf-8", errors="replace"):
        line = raw.strip()
        match = SECTION_RE.match(line)
        if match and "window=" not in line:
            key = f"{match.group('rot')}:{match.group('mode')}"
            current = {
                "rot": match.group("rot"),
                "step": int(match.group("step")),
                "mode": match.group("mode"),
                "uapp": [],
                "up10d": [],
                "lcd": [],
                "scap": [],
            }
            sections[key] = current
            continue
        if current is None:
            continue
        if "UAPP " in line:
            current["uapp"].append(parse_kv(line))
        elif "UP10D " in line:
            current["up10d"].append(parse_kv(line))
        elif "ULCD " in line:
            words = lcd_words(line)
            if words:
                current["lcd"].append(words)
        elif "SCAP " in line:
            current["scap"].append(parse_kv(line))
    return sections


def seq(rows, field, limit=8):
    values = []
    for row in rows:
        value = row.get(field)
        if value is not None and (not values or values[-1] != value):
            values.append(value)
    if len(values) > limit:
        return ",".join(values[:limit]) + ",..."
    return ",".join(values) if values else "-"


def last(rows, field):
    for row in reversed(rows):
        if field in row:
            return row[field]
    return "-"


def lcd_diff_summary(a, b):
    if not a or not b:
        return "missing"
    aw = a[-1]
    bw = b[-1]
    diff = []
    for idx, (left, right) in enumerate(zip(aw, bw)):
        if left != right:
            try:
                mask = int(left, 16) ^ int(right, 16)
                diff.append(f"{idx}:{mask:08X}")
            except ValueError:
                diff.append(f"{idx}:?")
    return "same" if not diff else " ".join(diff[:6]) + (" ..." if len(diff) > 6 else "")


def unique_lcd_count(rows):
    return len({tuple(row) for row in rows})


def compare(stock_path, custom_path, out_path):
    stock = parse(stock_path)
    custom = parse(custom_path)
    keys = sorted(set(stock) | set(custom))
    lines = [
        "# Rotary/Mode Comparison",
        "",
        f"Stock: `{stock_path}`",
        f"Custom: `{custom_path}`",
        "",
        "| Rotary/Mode | Stock Range | Custom Range | Status/R1E/R1F/R20/R29 | LCD Diff final XOR | LCD uniq S/C | SCAP S/C | Notable |",
        "|---|---:|---:|---|---|---:|---:|---|",
    ]
    for key in keys:
        s = stock.get(key)
        c = custom.get(key)
        if not s or not c:
            lines.append(f"| `{key}` | {'present' if s else '-'} | {'present' if c else '-'} | - | - | - | - | missing section |")
            continue
        fields = ["status", "r1e", "r1f", "r20", "r29"]
        pairs = []
        notable = []
        for field in fields:
            sv = last(s["up10d"], field)
            cv = last(c["up10d"], field)
            pairs.append(f"{field}:{sv}/{cv}")
            if sv != "-" and cv != "-" and sv != cv:
                notable.append(field)
        s_range = seq(s["up10d"] or s["uapp"], "range")
        c_range = seq(c["up10d"] or c["uapp"], "range")
        if s_range != c_range:
            notable.append("range")
        lcd_diff = lcd_diff_summary(s["lcd"], c["lcd"])
        if lcd_diff != "same":
            notable.append("lcd")
        scap_s = len(s["scap"])
        scap_c = len(c["scap"])
        if scap_s != scap_c:
            notable.append("scap")
        s_val = last(s["up10d"], "val")
        c_val = last(c["up10d"], "val")
        if s_val != "-" and c_val != "-" and s_val != c_val:
            notable.append(f"val {s_val}/{c_val}")
        note = ", ".join(dict.fromkeys(notable)) if notable else "ok"
        lines.append(
            f"| `{key}` | `{s_range}` | `{c_range}` | `{' '.join(pairs)}` | `{lcd_diff}` | "
            f"{unique_lcd_count(s['lcd'])}/{unique_lcd_count(c['lcd'])} | {scap_s}/{scap_c} | {note} |"
        )
    with open(out_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))
        f.write("\n")
    print(out_path)


if __name__ == "__main__":
    if len(sys.argv) != 4:
        raise SystemExit("usage: compare_rotary_logs.py STOCK_LOG CUSTOM_LOG OUT_MD")
    compare(sys.argv[1], sys.argv[2], sys.argv[3])
