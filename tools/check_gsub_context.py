"""Optional FontTools diagnostics for NexaUI's generated contextual fixtures.

This reads tables independently; it is not a general reference shaper. The
optional real-font check is a deliberately constrained A + acute context.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

import fontTools
from fontTools.ttLib import TTFont


def unwrap(lookup, sub):
    return (sub.ExtensionLookupType, sub.ExtSubTable) if lookup.LookupType == 7 else (lookup.LookupType, sub)


def fixtures(directory):
    results = []
    for kind in (5, 6):
        for form in (1, 2, 3):
            path = directory / f"context-{kind}-{form}.ttf"
            with TTFont(path) as font:
                lookups = font["GSUB"].table.LookupList.Lookup
                root = lookups[0]
                sub = root.SubTable[0]
                assert root.LookupType == kind and sub.Format == form
                if form == 3:
                    pattern = sub
                    coverage = sub.Coverage if kind == 5 else sub.InputCoverage
                    assert [c.glyphs for c in coverage] == [["B"], ["C"]]
                    if kind == 6:
                        assert [c.glyphs for c in sub.BacktrackCoverage] == [["A"], ["F"]]
                        assert [c.glyphs for c in sub.LookAheadCoverage] == [["D"], ["E"]]
                else:
                    assert sub.Coverage.glyphs == ["B"]
                    if form == 1:
                        sets = sub.SubRuleSet if kind == 5 else sub.ChainSubRuleSet
                        item = sets[0]
                        pattern = (item.SubRule if kind == 5 else item.ChainSubRule)[0]
                        assert pattern.Input == ["C"]
                    else:
                        classes = sub.ClassDef if kind == 5 else sub.InputClassDef
                        assert classes.classDefs.get("A", 0) == 0
                        item = (sub.SubClassSet if kind == 5 else sub.ChainSubClassSet)[classes.classDefs["B"]]
                        pattern = (item.SubClassRule if kind == 5 else item.ChainSubClassRule)[0]
                        assert (pattern.Class if kind == 5 else pattern.Input) == [classes.classDefs["C"]]
                    if kind == 6:
                        expected_back = ["A", "F"] if form == 1 else [sub.BacktrackClassDef.classDefs[g] for g in ("A", "F")]
                        expected_ahead = ["D", "E"] if form == 1 else [sub.LookAheadClassDef.classDefs[g] for g in ("D", "E")]
                        assert pattern.Backtrack == expected_back and pattern.LookAhead == expected_ahead
                records = [(r.SequenceIndex, r.LookupListIndex) for r in pattern.SubstLookupRecord]
                assert records == [(1, 1), (0, 2)]
                assert lookups[1].SubTable[0].mapping == {"C": "F"}
                assert lookups[2].SubTable[0].mapping == {"B": "E"}
                results.append({"type": kind, "format": form, "sha256": hashlib.sha256(path.read_bytes()).hexdigest(), "records": records, "matches": True})
    return results


def real_context(path, probe):
    with TTFont(path) as font:
        table = font["GSUB"].table
        script = next(s.Script for s in table.ScriptList.ScriptRecord if s.ScriptTag == "latn")
        active = set()
        for i in script.DefaultLangSys.FeatureIndex:
            feature = table.FeatureList.FeatureRecord[i]
            if feature.FeatureTag in ("ccmp", "locl", "rlig", "liga", "clig", "calt"):
                active.update(feature.Feature.LookupListIndex)
        cmap = font.getBestCmap()
        base, mark = cmap[ord("A")], cmap[0x301]
        expected = None
        for index in sorted(active):
            lookup = table.LookupList.Lookup[index]
            if lookup.LookupFlag:
                continue
            for sub in lookup.SubTable:
                kind, sub = unwrap(lookup, sub)
                if kind != 6 or sub.Format != 3 or sub.BacktrackGlyphCount != 1 or sub.InputGlyphCount != 1 or sub.LookAheadGlyphCount:
                    continue
                if base not in sub.BacktrackCoverage[0].glyphs or mark not in sub.InputCoverage[0].glyphs or len(sub.SubstLookupRecord) != 1:
                    continue
                action = sub.SubstLookupRecord[0]
                if action.SequenceIndex:
                    continue
                nested = table.LookupList.Lookup[action.LookupListIndex]
                for item in nested.SubTable:
                    nested_kind, item = unwrap(nested, item)
                    if nested_kind == 1 and mark in item.mapping:
                        expected = {"root": index, "nested": action.LookupListIndex, "base": font.getGlyphID(base), "input_mark": font.getGlyphID(mark), "output_mark": font.getGlyphID(item.mapping[mark])}
                        break
                if expected:
                    break
            if expected:
                break
        if expected is None:
            raise ValueError("font has no supported simple A + acute context for this diagnostic")
    process = subprocess.run([str(probe.resolve()), str(path.resolve())], text=True, capture_output=True, timeout=30)
    rows = [line.split(",") for line in process.stdout.splitlines()]
    glyphs = [int(r[3]) for r in rows if r[0] == "glyph" and int(r[1]) == 0]
    assert process.returncode == 0 and glyphs == [expected["base"], expected["output_mark"]], (process.returncode, glyphs, expected)
    return {"font": path.name, "sha256": hashlib.sha256(path.read_bytes()).hexdigest(), "trace": expected, "actual_glyphs": glyphs, "matches": True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixtures", type=Path, required=True)
    parser.add_argument("--font", type=Path)
    parser.add_argument("--probe", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = {"fonttools": fontTools.__version__, "fixtures": fixtures(args.fixtures)}
    if args.font:
        if not args.probe:
            parser.error("--font requires --probe")
        result["real_context"] = real_context(args.font, args.probe)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(f"FontTools: {len(result['fixtures'])}/6 contextual formats; real context: {'matches' if result.get('real_context') else 'not requested'}")


if __name__ == "__main__":
    main()
