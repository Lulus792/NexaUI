"""Create deterministic text-layout seeds from text corpus and edge cases."""

from pathlib import Path


def main() -> None:
    corpus = Path(__file__).parent / "corpus"
    source = corpus / "text"
    output = corpus / "layout"
    output.mkdir(parents=True, exist_ok=True)
    for seed in sorted(source.iterdir()):
        if seed.is_file():
            (output / seed.name).write_bytes(seed.read_bytes())
    extras = {
        "wrap.txt": "0 1 5 * 0 1 5 *\n0 1 5 *",
        "hard-breaks.txt": "A\r\n\nB\u2028C\u2029",
        "paragraph-separators.txt": "A\u001cB\u001dC\u001eD",
        "bidi-controls.txt": "abc \u2067אבג 123\u2069 xyz",
        "marks-joiners.txt": "a\u0301 👩🏽\u200d💻 क्\u200dष",
        "spaces.txt": "  A   B\tC\u00a0D\u200bE  ",
    }
    for name, text in extras.items():
        (output / name).write_bytes(text.encode("utf-8"))


if __name__ == "__main__":
    main()
