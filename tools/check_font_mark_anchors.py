"""Compare two-glyph LTR/RTL mark anchors using optional FontTools and a probe.

This is a local font diagnostic, not a complete reference shaper. FontTools is
only needed to run this script; NexaUI and the C probe do not depend on it.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def anchor_xy(anchor, ppem, scale):
    result = [anchor.XCoordinate * scale, anchor.YCoordinate * scale]
    if anchor.Format == 3:
        for axis, name in enumerate(("XDeviceTable", "YDeviceTable")):
            device = getattr(anchor, name, None)
            if device and device.DeltaFormat in (1, 2, 3) and device.StartSize <= ppem <= device.EndSize:
                result[axis] += device.DeltaValue[ppem - device.StartSize]
    return result


def compare(font, rows, sample, script_tag):
    pair = [row for row in rows if row[0] == "glyph" and int(row[1]) == sample]
    if len(pair) != 2:
        raise ValueError(f"sample {sample}: expected two glyphs")
    names = [font.getGlyphName(int(row[3])) for row in pair]
    table = font["GPOS"].table
    scripts = {entry.ScriptTag: entry.Script for entry in table.ScriptList.ScriptRecord}
    script = scripts.get(script_tag, scripts.get("DFLT"))
    if not script or not script.DefaultLangSys:
        raise ValueError(f"no default language for {script_tag}")
    language = script.DefaultLangSys
    active = set()
    indices = list(language.FeatureIndex)
    if language.ReqFeatureIndex != 0xffff:
        indices.append(language.ReqFeatureIndex)
    for index in indices:
        feature = table.FeatureList.FeatureRecord[index]
        if index == language.ReqFeatureIndex or feature.FeatureTag in ("mark", "mkmk", "abvm", "blwm"):
            active.update(feature.Feature.LookupListIndex)
    expected = None
    scale = 32 / font["head"].unitsPerEm
    advance = font["hmtx"][names[0]][0] * scale
    if abs(float(pair[0][4]) - advance) > 1e-8 or any(abs(float(x)) > 1e-8 for x in pair[0][5:7]):
        raise ValueError("this limited comparison requires an unshifted base with its natural advance")
    if abs(float(pair[1][4])) > 1e-8:
        raise ValueError("this comparison requires a nonspacing mark")
    for index in sorted(active):
        lookup = table.LookupList.Lookup[index]
        for subtable in lookup.SubTable:
            kind = lookup.LookupType
            if kind == 9:
                kind, subtable = subtable.ExtensionLookupType, subtable.ExtSubTable
            if kind != 4 or names[0] not in subtable.BaseCoverage.glyphs or names[1] not in subtable.MarkCoverage.glyphs:
                continue
            # These probes have one base followed by one mark: filtering a base
            # class excludes that candidate; mark-set filters do not exclude it.
            gdef = font["GDEF"].table if "GDEF" in font else None
            classes = getattr(gdef, "GlyphClassDef", None)
            glyph_class = classes.classDefs.get(names[0], 0) if classes else 0
            if (glyph_class == 1 and lookup.LookupFlag & 2) or (glyph_class == 2 and lookup.LookupFlag & 4):
                continue
            mark = subtable.MarkArray.MarkRecord[subtable.MarkCoverage.glyphs.index(names[1])]
            base = subtable.BaseArray.BaseRecord[subtable.BaseCoverage.glyphs.index(names[0])].BaseAnchor[mark.Class]
            if base is None:
                continue
            b, m = anchor_xy(base, 32, scale), anchor_xy(mark.MarkAnchor, 32, scale)
            expected = [b[0] - m[0] - (advance if sample == 0 else 0), b[1] - m[1]]
            break
    if expected is None:
        raise ValueError(f"no supported active mark-to-base anchor for {names}")
    actual = [float(x) for x in pair[1][5:7]]
    return {"sample": sample, "glyphs": names, "expected": expected, "actual": actual,
            "matches": all(abs(a-b) < 1e-8 for a, b in zip(expected, actual))}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    parser.add_argument("fonts", type=Path, nargs="+")
    args = parser.parse_args()
    try:
        import fontTools
        from fontTools.ttLib import TTFont
    except ImportError:
        parser.error("install optional FontTools in the diagnostic Python environment")
    results = {"fonttools": fontTools.__version__, "ppem": 32, "fonts": []}
    failed = False
    for path in args.fonts:
        item = {"font": path.name, "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
        try:
            process = subprocess.run([str(args.probe.resolve()), str(path.resolve())],
                                     capture_output=True, text=True, timeout=30, check=False)
            item.update(probe_exit=process.returncode, stdout=process.stdout, stderr=process.stderr)
            if process.returncode:
                raise ValueError("font probe rejected a sample")
            rows = [line.split(",") for line in process.stdout.splitlines()]
            with TTFont(path) as font:
                item["version"] = font["name"].getDebugName(5)
                item["anchors"] = [compare(font, rows, 0, "latn"), compare(font, rows, 3, "hebr")]
            failed |= not all(result["matches"] for result in item["anchors"])
        except (ValueError, OSError, subprocess.TimeoutExpired) as error:
            item["error"] = str(error)
            failed = True
        results["fonts"].append(item)
    output = json.dumps(results, indent=2) + "\n"
    if args.output:
        args.output.write_text(output, encoding="utf-8")
    print(output, end="")
    return int(failed)


if __name__ == "__main__":
    raise SystemExit(main())
