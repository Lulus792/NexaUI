"""List Unicode Script property values present in the text fuzz seeds."""

from bisect import bisect_right
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = ROOT / "data" / "unicode" / "18.0.0" / "Scripts.txt"
CORPUS = ROOT / "fuzz" / "corpus" / "text"


def script_ranges() -> tuple[list[int], list[tuple[int, str]]]:
    ranges = []
    for raw in SCRIPTS.read_text(encoding="utf-8").splitlines():
        field = raw.split("#", 1)[0].strip()
        if not field:
            continue
        codepoints, name = (part.strip() for part in field.split(";", 1))
        start, _, end = codepoints.partition("..")
        ranges.append((int(start, 16), int(end or start, 16), name))
    ranges.sort()
    return [start for start, _, _ in ranges], ranges


def main() -> None:
    starts, ranges = script_ranges()
    observed: dict[str, list[str]] = {}
    invalid = []
    for path in sorted(CORPUS.iterdir()):
        if not path.is_file():
            continue
        try:
            text = path.read_bytes().decode("utf-8")
        except UnicodeDecodeError:
            invalid.append(path.name)
            continue
        for char in text:
            codepoint = ord(char)
            pos = bisect_right(starts, codepoint) - 1
            script = (ranges[pos][2] if pos >= 0 and
                      codepoint <= ranges[pos][1] else "Unknown")
            if script not in ("Common", "Inherited", "Unknown"):
                names = observed.setdefault(script, [])
                if path.name not in names:
                    names.append(path.name)
    all_scripts = {name for _, _, name in ranges} - {
        "Common", "Inherited", "Unknown"}
    print(f"Unicode 18.0.0: {len(observed)} of {len(all_scripts)} assigned scripts occur in valid text seeds")
    for script, names in sorted(observed.items()):
        print(f"{script}: {', '.join(names)}")
    print(f"Malformed UTF-8 seeds: {', '.join(invalid) or 'none'}")
    print(f"Assigned scripts absent from seeds: {', '.join(sorted(all_scripts - observed.keys()))}")


if __name__ == "__main__":
    main()
