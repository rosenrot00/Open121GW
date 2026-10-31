#!/usr/bin/env python3
import re
import sys
from pathlib import Path


FIELDS = [
    "mode", "range",
    "tw", "flags", "sfl", "val", "disp", "inst", "tval", "tu", "zero", "fref",
    "scale", "raw", "av", "aa", "alo", "ahi", "af", "at", "ax",
    "db", "c6", "c7", "b0", "b1",
]

DUAL_CURRENT_MODES = {"0D", "0E", "0F", "16", "17", "18"}


def parse_kv(line):
    out = {}
    for key, value in re.findall(r"([A-Za-z0-9_]+)=([0-9A-Fa-fx?/.-]+)", line):
        out[key] = value
    return out


def parse_capture(path):
    sections = {}
    rotary = None
    current = None
    for line in Path(path).read_text(errors="replace").splitlines():
        if line.startswith("# ") and " rot=" in line:
            rotary = line[2:].strip()
            current = rotary
            sections.setdefault(current, {"p10": [], "lcd": [], "ufe": [], "ufecfg": []})
            continue
        if line.startswith("## ") and " mode=" in line and rotary:
            label = line[3:].strip()
            if " window=" not in label and "cycle-repeat" not in label:
                current = f"{rotary} / {label}"
                sections.setdefault(current, {"p10": [], "lcd": [], "ufe": [], "ufecfg": []})
            continue
        if line.startswith("## ") and " transition=" in line:
            current = None
            continue
        if current is None:
            continue
        if "UP10D mode=" in line:
            sections[current]["p10"].append(parse_kv(line))
        elif "UFECFG " in line:
            sections[current]["ufecfg"].append(parse_kv(line))
        elif "UFE " in line:
            sections[current]["ufe"].append(parse_kv(line))
        elif "ULCD " in line:
            lcd = line.split("ULCD ", 1)[1].strip()
            sections[current]["lcd"].append(lcd)
    return sections


def last(items):
    return items[-1] if items else {}


def longest_cfg(items):
    best = {}
    for item in items:
        if len(item.get("cfg", "")) >= len(best.get("cfg", "")):
            best = item
    return best


def cfg_set(items):
    return {item.get("cfg", "") for item in items if item.get("cfg", "")}


def unique_last(items):
    seen = []
    for item in items:
        if item not in seen:
            seen.append(item)
    return seen[-1] if seen else ""


def parse_lcd_words(text):
    words = text.split()
    if len(words) != 16:
        return None
    try:
        return [int(word, 16) for word in words]
    except ValueError:
        return None


def normalize_lcd(text, mask_upper=True, mask_lower=False):
    words = parse_lcd_words(text)
    if words is None:
        return text

    if mask_upper:
        # The upper five-character field normally carries live device
        # temperature. Mask those character segments, their decimal points and
        # secondary sign, while keeping annunciators comparable.
        char_mask = 0x1FF80000
        for idx in (0, 2, 4, 6):
            words[idx] &= ~char_mask
        words[4] &= ~0x20000000
        for idx in (1, 3, 5, 7):
            words[idx] &= ~0x00000100

    if mask_lower:
        # Lower five-character field and decimal/minus bits are the measured
        # value. Keep mode icons, units and bargraph bits for semantic compare.
        lower_char_masks = {
            0: 0x0000F000,
            1: 0x0000007E,
            2: 0x0000F000,
            3: 0x000000FE,
            4: 0x0000F000,
            5: 0x000000FE,
            6: 0x0000F000,
            7: 0x000000FE,
        }
        for idx, mask in lower_char_masks.items():
            words[idx] &= ~mask
        for idx, mask in (
            (0, 0x000000FC),
            (2, 0x000000FE),
            (4, 0x000000FE),
            (6, 0x000000FE),
        ):
            words[idx] &= ~mask
    return " ".join(f"{word:08X}" for word in words)


def diff_fields(stock, custom):
    diffs = []
    mode = stock.get("mode") or custom.get("mode")
    freq_group = mode in ("06", "07", "08")
    freq_group_skip = {"status", "sf", "zero", "gain", "scale", "raw", "av", "aa", "alo", "ahi", "af", "at", "ax"}
    dual_current_skip = {"zero", "gain", "scale"}
    for field in FIELDS:
        if freq_group and field in freq_group_skip:
            continue
        if mode in DUAL_CURRENT_MODES and field in dual_current_skip:
            continue
        sv = stock.get(field, "")
        cv = custom.get(field, "")
        if not sv and not cv:
            continue
        if not sv or not cv:
            if field not in {"mode", "range", "val", "tval", "tu", "fref"}:
                continue
        if field == "scale" and not sv:
            # Older stock debug payloads print gain as float bits but not the
            # integer gain scale, so this field is not comparable there.
            continue
        if stock.get("mode") == "05" and custom.get("mode") == "05":
            if field == "val" and custom.get("tval"):
                cv = custom.get("tval", "")
            elif field in ("disp", "inst", "tval", "tu"):
                continue
        if sv != cv:
            diffs.append(f"{field}: {sv or '-'} != {cv or '-'}")
    return "; ".join(diffs)


def main():
    if len(sys.argv) != 4:
        print("usage: compare_rotary_capture.py STOCK_LOG CUSTOM_LOG OUT_MD", file=sys.stderr)
        return 2
    stock = parse_capture(sys.argv[1])
    custom = parse_capture(sys.argv[2])
    names = sorted(set(stock) | set(custom))
    rows = [
        "# rotary capture comparison",
        "",
        f"stock={sys.argv[1]}",
        f"custom={sys.argv[2]}",
        "",
        "| section | stock mode/range | custom mode/range | stock frontend | custom frontend | value | lcd | differences |",
        "|---|---:|---:|---|---|---|---|---|",
    ]
    for name in names:
        sp = last(stock.get(name, {}).get("p10", []))
        cp = last(custom.get(name, {}).get("p10", []))
        su = last(stock.get(name, {}).get("ufe", []))
        cu = last(custom.get(name, {}).get("ufe", []))
        scfg = longest_cfg(stock.get(name, {}).get("ufecfg", []))
        ccfg = longest_cfg(custom.get(name, {}).get("ufecfg", []))
        sl = unique_last(stock.get(name, {}).get("lcd", []))
        cl = unique_last(custom.get(name, {}).get("lcd", []))
        s_mode = f"{sp.get('mode','?')}/{sp.get('range','?')}"
        c_mode = f"{cp.get('mode','?')}/{cp.get('range','?')}"
        s_fe = f"p10 r1f={sp.get('r1f','-')} r20={sp.get('r20','-')} live r1f={su.get('r1f','-')} r20={su.get('r20','-')} cfg={scfg.get('cfg','-')}"
        c_fe = f"p10 r1f={cp.get('r1f','-')} r20={cp.get('r20','-')} live r1f={cu.get('r1f','-')} r20={cu.get('r20','-')} cfg={ccfg.get('cfg','-')}"
        custom_value = cp.get("tval") if sp.get("mode") == "05" and cp.get("mode") == "05" and cp.get("tval") else cp.get("val", cp.get("disp","-"))
        value = f"{sp.get('val', sp.get('disp','-'))} / {custom_value}"
        lcd_same = sl == cl
        lcd_main_same = normalize_lcd(sl) == normalize_lcd(cl)
        lcd_icons_same = normalize_lcd(sl, mask_upper=True, mask_lower=True) == normalize_lcd(cl, mask_upper=True, mask_lower=True)
        if lcd_same:
            lcd = "same"
        elif lcd_main_same:
            lcd = "upper-temp-only"
        elif lcd_icons_same:
            lcd = "value-only"
        else:
            lcd = "diff"
        diffs = diff_fields(sp, cp)
        for field in ("r1f", "r20", "r29"):
            if su.get(field, "") != cu.get(field, ""):
                diffs = (diffs + "; " if diffs else "") + f"live {field}: {su.get(field,'-') or '-'} != {cu.get(field,'-') or '-'}"
        s_ctrl = scfg.get("ctrl", "")
        c_ctrl = ccfg.get("ctrl", "")
        if s_ctrl and c_ctrl:
            # ctrl[0] is the volatile ready/status latch; ctrl[1] is the mask.
            s_mask = s_ctrl.split("/")[-1]
            c_mask = c_ctrl.split("/")[-1]
            if s_mask != c_mask:
                diffs = (diffs + "; " if diffs else "") + f"ctrl mask: {s_mask or '-'} != {c_mask or '-'}"
        s_cfg_set = cfg_set(stock.get(name, {}).get("ufecfg", []))
        c_cfg_set = cfg_set(custom.get(name, {}).get("ufecfg", []))
        if s_cfg_set or c_cfg_set:
            cfg_diff = s_cfg_set != c_cfg_set
        else:
            cfg_diff = scfg.get("cfg", "") != ccfg.get("cfg", "")
        if cfg_diff:
            diffs = (diffs + "; " if diffs else "") + "frontend cfg differs"
        if not lcd_same and not lcd_main_same and not lcd_icons_same:
            diffs = (diffs + "; " if diffs else "") + "lcd shadow differs"
        rows.append(f"| {name} | {s_mode} | {c_mode} | `{s_fe}` | `{c_fe}` | `{value}` | {lcd} | {diffs or 'same'} |")
    Path(sys.argv[3]).write_text("\n".join(rows) + "\n")
    print(sys.argv[3])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
