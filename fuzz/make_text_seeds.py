"""Write deterministic UTF-8 and malformed-byte seeds for text_fuzz."""

from pathlib import Path


SEEDS = {
    "empty.bin": b"",
    "latin.txt": "Cafe\u0301 — fi fl\nsecond line".encode("utf-8"),
    "greek-cyrillic.txt": "Ελληνικά Русский текст".encode("utf-8"),
    "arabic-hebrew.txt": "العَرَبِيَّة עברית (123)".encode("utf-8"),
    "indic.txt": "हिन्दी বাংলা தமிழ் తెలుగు".encode("utf-8"),
    "southeast-asian.txt": "ภาษาไทย ខ្មែរ မြန်မာ".encode("utf-8"),
    "east-asian.txt": "中文 日本語 ひらがな カタカナ 한국어".encode("utf-8"),
    "more-scripts.txt": "Հայերեն ქართული አማርኛ ລາວ བོད".encode("utf-8"),
    "emoji.txt": "👩🏽‍💻 👨‍👩‍👧‍👦 🇩🇪 ❤️".encode("utf-8"),
    "embedded-nul.bin": b"a\x00b\r\nc",
    "malformed.bin": b"\xf0\x28\x8c\x28\xc0\xaf",
}


def main() -> None:
    output = Path(__file__).parent / "corpus" / "text"
    output.mkdir(parents=True, exist_ok=True)
    for name, data in SEEDS.items():
        (output / name).write_bytes(data)


if __name__ == "__main__":
    main()
