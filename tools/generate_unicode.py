#!/usr/bin/env python3
"""Generate pinned, compact Unicode property tables for NexaUI.

Usage: python tools/generate_unicode.py [--check]
No network access or third-party packages are required.
"""

from __future__ import annotations

import argparse
import hashlib
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / "data" / "unicode" / "18.0.0"
MAX_CODEPOINT = 0x10FFFF
HASHES = {
    "BidiBrackets.txt": "4b3b62e4a14b84ee752808c810c602534921c09a4a1bf78cfbee566d66c125b3",
    "BidiCharacterTest.txt": "045b24d2c8ab066951bd32fe8c6b4de34647f72b5b1c7df0265f24ab53573e01",
    "BidiTest.txt": "9af2f882a4ab50912e388f069a673b94eacd82fa6d07d20a3ff7f3c759e905aa",
    "Scripts.txt": "0071fd81b6aeae25f6e8bce8efec3066a6476a91b49bdb2f52dc76e817862a6a",
    "PropertyValueAliases.txt": "06c4c8eaf7b0bf34abe73b113da1215bd784ac254d4c223600b90267caa4bbbd",
    "auxiliary/GraphemeBreakTest.txt": "b0cf047ee94485bbdc846de2b902f5f8a815f6b674f9d04223cddadd91c9df31",
    "auxiliary/WordBreakTest.txt": "3dd70c071781276067c680d87303f60434adce7b067bd063194af374edad86a5",
    "auxiliary/LineBreakTest.txt": "fd9ff9eca411080085b41471bba79ae19d9db2e16924f9770b8509f1affd3f95",
    "DerivedCoreProperties.txt": "09c928886a178fcafd93c29e4bd59073a058e5a100b716d425cb563ab50f68c9",
    "LineBreak.txt": "ae8cf1970c73f3f1a12d77852df96c4b2b1723ca8b388e36bb5739250c6849de",
    "EastAsianWidth.txt": "a0cf29eacd00cfcaec4381c6b7c281685f18dbb4e7ff82b4076ccb342ca839aa",
    "auxiliary/GraphemeBreakProperty.txt": "0839dcb79e4ac639ecd538b1abf7c9d22e3f9dd265b7e182d33627aa4d75b45a",
    "auxiliary/WordBreakProperty.txt": "8dbfa17063e11084201f33c3e76d485d3b9166930c71db8e39ed1c9234171aec",
    "emoji/emoji-data.txt": "80d00f8e616a0ef27fd6b8de3b758c06383b5d917e2977709578e68baf733bf1",
    "extracted/DerivedBidiClass.txt": "d9e23222522551348ea1ccfbb4f62efbf98982afb95840f8959c08ed992c5607",
    "extracted/DerivedLineBreak.txt": "e015a6d4bde8dd0f8b72bf15ac9a4f588146ce6bc0bf8980e9dae8302cdbdc28",
    "extracted/DerivedEastAsianWidth.txt": "f4c7bc4537a71e12452f6347ecbbc71c31cba21b5190c92be1ae296b0ef32b09",
    "extracted/DerivedGeneralCategory.txt": "d6b151d2d40ee9b1876d26f417980f45ffae47b6055ccf7203cb31f07a030f94",
    "license.txt": "e7a93b009565cfce55919a381437ac4db883e9da2126fa28b91d12732bc53d96",
}
SOURCES = (
    ("gcb", "auxiliary/GraphemeBreakProperty.txt", None, "Other"),
    ("wb", "auxiliary/WordBreakProperty.txt", None, "Other"),
    ("lb", "extracted/DerivedLineBreak.txt", None, "XX"),
    ("gc", "extracted/DerivedGeneralCategory.txt", None, "Cn"),
    ("eaw", "extracted/DerivedEastAsianWidth.txt", None, "N"),
    ("ep", "emoji/emoji-data.txt", "Extended_Pictographic", "No"),
    ("incb", "DerivedCoreProperties.txt", "InCB", "None"),
    ("di", "DerivedCoreProperties.txt", "Default_Ignorable_Code_Point", "No"),
    ("bidi", "extracted/DerivedBidiClass.txt", None, "L"),
    ("script", "Scripts.txt", None, "XX"),
)


def source_text(relative: str) -> str:
    raw = (DATA / relative).read_bytes()
    actual = hashlib.sha256(raw).hexdigest()
    if actual != HASHES[relative]:
        raise ValueError(f"SHA-256 mismatch for {relative}: {actual}")
    return raw.decode("utf-8-sig")


def codepoint_range(field: str) -> tuple[int, int]:
    match = re.fullmatch(r"([0-9A-Fa-f]{4,6})(?:\.\.([0-9A-Fa-f]{4,6}))?", field.strip())
    if not match:
        raise ValueError(f"invalid code point range: {field}")
    first = int(match[1], 16)
    last = int(match[2], 16) if match[2] else first
    if first > last or last > MAX_CODEPOINT:
        raise ValueError(f"out-of-range code point: {field}")
    return first, last


def parse_source(relative: str, selector: str | None) -> tuple[list[tuple[int, int, str]], list[tuple[int, int, str]]]:
    defaults: list[tuple[int, int, str]] = []
    explicit: list[tuple[int, int, str]] = []
    aliases = {"Left_To_Right": "L", "Right_To_Left": "R",
               "Arabic_Letter": "AL", "European_Terminator": "ET",
               "Unknown": "XX", "Prefix_Numeric": "PR",
               "Ideographic": "ID", "Neutral": "N", "Wide": "W"}
    for number, line in enumerate(source_text(relative).splitlines(), 1):
        if line.startswith("# @missing:"):
            fields = [field.strip() for field in line.split(":", 1)[1].split(";")]
            if selector is None and len(fields) == 2:
                value = aliases.get(fields[1], fields[1])
                defaults.append((*codepoint_range(fields[0]), value))
            elif selector is not None and len(fields) == 3 and fields[1] == selector:
                defaults.append((*codepoint_range(fields[0]), fields[2]))
            continue
        content = line.split("#", 1)[0].strip()
        if not content:
            continue
        fields = [field.strip() for field in content.split(";")]
        if len(fields) < 2:
            raise ValueError(f"{relative}:{number}: invalid property line")
        if selector is None and len(fields) == 2:
            value = fields[1]
        elif selector == "InCB" and len(fields) == 3 and fields[1] == selector:
            value = fields[2]
        elif selector not in (None, "InCB") and fields[1] == selector:
            value = "Yes"
        else:
            continue
        explicit.append((*codepoint_range(fields[0]), value))
    if not explicit:
        raise ValueError(f"{relative}: no property entries")
    return defaults, explicit


def enum_name(prefix: str, value: str) -> str:
    words = re.sub(r"(?<=[a-z0-9])(?=[A-Z])", "_", value)
    return "NX_UCD_" + prefix.upper() + "_" + words.upper().replace("-", "_")


def make_table(prefix: str, relative: str, selector: str | None, fallback: str):
    defaults, explicit = parse_source(relative, selector)
    values = [fallback] + sorted({entry[2] for entry in defaults + explicit} - {fallback})
    if len(values) > 255:
        raise ValueError(f"too many {prefix} property values")
    by_name = {name: index for index, name in enumerate(values)}
    table = bytearray(MAX_CODEPOINT + 1)
    for first, last, value in defaults:
        table[first:last + 1] = bytes([by_name[value]]) * (last - first + 1)
    seen = bytearray(MAX_CODEPOINT + 1)
    for first, last, value in explicit:
        if any(seen[first:last + 1]):
            raise ValueError(f"overlapping explicit {prefix} ranges at U+{first:04X}")
        seen[first:last + 1] = b"\1" * (last - first + 1)
        table[first:last + 1] = bytes([by_name[value]]) * (last - first + 1)
    ranges = []
    start = None
    prior = 0
    for codepoint, value in enumerate(table):
        if value != prior:
            if start is not None:
                ranges.append((start, codepoint - 1, prior))
            start = codepoint if value else None
            prior = value
    if start is not None:
        ranges.append((start, MAX_CODEPOINT, prior))
    return values, ranges


def generate() -> tuple[str, str]:
    for relative in HASHES:
        source_text(relative)
    tables = [(prefix, *make_table(prefix, relative, selector, fallback))
              for prefix, relative, selector, fallback in SOURCES]
    preamble = ("/* Generated by tools/generate_unicode.py from Unicode 18.0.0 UCD.\n"
                "   Unicode data copyright Unicode, Inc.; see data/unicode/18.0.0/license.txt.\n"
                "   Do not edit by hand. */\n")
    header = [preamble, "#ifndef NEXAUI_UNICODE_DATA_H\n#define NEXAUI_UNICODE_DATA_H\n",
              "#include <stdint.h>\n", "#define NX_UCD_VERSION_MAJOR 18u\n",
              "#define NX_UCD_VERSION_MINOR 0u\n#define NX_UCD_VERSION_PATCH 0u\n"]
    body = [preamble, '#include "unicode_data.h"\n#include <stddef.h>\n',
            "typedef struct nx_ucd_range { uint32_t first, last; uint8_t value; } nx_ucd_range;\n",
            "static uint8_t nx_ucd_find(const nx_ucd_range *ranges, size_t count, uint32_t scalar) {\n",
            "    size_t low = 0, high = count;\n",
            "    while (low < high) {\n",
            "        size_t middle = low + (high - low) / 2u;\n",
            "        if (scalar < ranges[middle].first) high = middle;\n",
            "        else if (scalar > ranges[middle].last) low = middle + 1u;\n",
            "        else return ranges[middle].value;\n",
            "    }\n    return 0;\n}\n"]
    for prefix, values, ranges in tables:
        header.append(f"enum nx_ucd_{prefix}_class {{\n")
        for index, value in enumerate(values):
            comma = "," if index + 1 < len(values) else ""
            header.append(f"    {enum_name(prefix, value)} = {index}{comma}\n")
        header.append("};\n")
        header.append(f"uint8_t nx_ucd_{prefix}(uint32_t scalar);\n")
        body.append(f"static const nx_ucd_range nx_ucd_{prefix}_ranges[] = {{\n")
        for first, last, value in ranges:
            body.append(f"    {{0x{first:X}u, 0x{last:X}u, {value}u}},\n")
        body.append("};\n")
        # Boundary rebuilds repeatedly classify common ASCII scalars. Derive
        # the direct table from the same pinned ranges as the general lookup.
        ascii_values = [0] * 128
        for first, last, value in ranges:
            for scalar in range(first, min(last, 127) + 1):
                ascii_values[scalar] = value
        body.append(f"static const uint8_t nx_ucd_{prefix}_ascii[128] = {{\n")
        for offset in range(0, 128, 16):
            body.append("    " + ", ".join(f"{value}u" for value in
                        ascii_values[offset:offset + 16]) + ",\n")
        body.append("};\n")
        body.append(f"uint8_t nx_ucd_{prefix}(uint32_t scalar) {{\n")
        body.append(f"    if (scalar < 128u) return nx_ucd_{prefix}_ascii[scalar];\n")
        body.append(f"    return nx_ucd_find(nx_ucd_{prefix}_ranges,\n")
        body.append(f"        sizeof(nx_ucd_{prefix}_ranges) / sizeof(nx_ucd_{prefix}_ranges[0]), scalar);\n")
        body.append("}\n")
        if prefix == "script":
            aliases = {}
            for line in source_text("PropertyValueAliases.txt").splitlines():
                fields = [field.strip() for field in line.split("#", 1)[0].split(";")]
                if len(fields) >= 3 and fields[0] == "sc":
                    aliases[fields[2]] = fields[1]
            aliases["XX"] = "Zzzz"
            if any(name not in aliases or len(aliases[name]) != 4 for name in values):
                raise ValueError("missing four-character script alias")
            header.append("uint32_t nx_ucd_script_tag(uint32_t scalar);\n")
            body.append("static const uint32_t nx_ucd_script_tags[] = {\n")
            for name in values:
                tag = aliases[name]
                body.append(f"    0x{int.from_bytes(tag.encode('ascii'), 'big'):08X}u,\n")
            body.append("};\nuint32_t nx_ucd_script_tag(uint32_t scalar) {\n")
            body.append("    return nx_ucd_script_tags[nx_ucd_script(scalar)];\n}\n")
    brackets = []
    for number, line in enumerate(source_text("BidiBrackets.txt").splitlines(), 1):
        content = line.split("#", 1)[0].strip()
        if not content:
            continue
        fields = [field.strip() for field in content.split(";")]
        if len(fields) != 3 or fields[2] not in ("o", "c"):
            raise ValueError(f"BidiBrackets.txt:{number}: invalid bracket")
        brackets.append((int(fields[0], 16), int(fields[1], 16), fields[2]))
    brackets.sort()
    if len({entry[0] for entry in brackets}) != len(brackets):
        raise ValueError("duplicate bidi bracket")
    header.append("/* Returns 1 for opening, 2 for closing, 0 otherwise. */\n")
    header.append("uint8_t nx_ucd_bidi_bracket(uint32_t scalar, uint32_t *paired);\n")
    body.append("typedef struct nx_ucd_bracket { uint32_t scalar, paired; uint8_t kind; } nx_ucd_bracket;\n")
    body.append("static const nx_ucd_bracket nx_ucd_brackets[] = {\n")
    for scalar, paired, kind in brackets:
        body.append(f"    {{0x{scalar:X}u, 0x{paired:X}u, {1 if kind == 'o' else 2}u}},\n")
    body.append("};\nuint8_t nx_ucd_bidi_bracket(uint32_t scalar, uint32_t *paired) {\n")
    body.append("    size_t low = 0, high = sizeof(nx_ucd_brackets) / sizeof(nx_ucd_brackets[0]);\n")
    body.append("    while (low < high) {\n")
    body.append("        size_t middle = low + (high - low) / 2u;\n")
    body.append("        if (scalar < nx_ucd_brackets[middle].scalar) high = middle;\n")
    body.append("        else if (scalar > nx_ucd_brackets[middle].scalar) low = middle + 1u;\n")
    body.append("        else {\n")
    body.append("            if (paired) *paired = nx_ucd_brackets[middle].paired;\n")
    body.append("            return nx_ucd_brackets[middle].kind;\n")
    body.append("        }\n    }\n    return 0;\n}\n")
    header.append("#endif\n")
    return "".join(header), "".join(body)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="fail if generated files differ")
    args = parser.parse_args()
    header, body = generate()
    for target, content in ((ROOT / "src/unicode_data.h", header),
                            (ROOT / "src/unicode_data.c", body)):
        if args.check:
            if not target.exists() or target.read_text(encoding="utf-8") != content:
                raise SystemExit(f"out of date: {target}")
        else:
            target.write_text(content, encoding="utf-8", newline="\n")
        print(f"{'checked' if args.check else 'generated'} {target.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
