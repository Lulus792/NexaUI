"""Create a compact, valid GSUB table for the libFuzzer corpus."""

from pathlib import Path
from struct import pack_into


data = bytearray(122)


def u16(offset: int, value: int) -> None:
    pack_into(">H", data, offset, value)


def u32(offset: int, value: int) -> None:
    pack_into(">I", data, offset, value)


def tag(offset: int, value: str) -> None:
    data[offset : offset + 4] = value.encode("ascii")


u32(0, 0x00010000)
u16(4, 10)
u16(6, 32)
u16(8, 58)
u16(10, 1)
tag(12, "latn")
u16(16, 8)
u16(18, 4)
u16(24, 0xFFFF)
u16(26, 2)
u16(28, 0)
u16(30, 1)
u16(32, 2)
tag(34, "ccmp")
u16(38, 14)
tag(40, "liga")
u16(44, 20)
u16(48, 1)
u16(54, 1)
u16(56, 1)
u16(58, 2)
u16(60, 6)
u16(62, 26)
u16(64, 1)
u16(68, 1)
u16(70, 8)
u16(72, 1)
u16(74, 6)
u16(76, 1)
u16(78, 1)
u16(80, 1)
u16(82, 5)
u16(84, 4)
u16(88, 1)
u16(90, 8)
u16(92, 1)
u16(94, 14)
u16(96, 1)
u16(98, 20)
u16(106, 1)
u16(108, 1)
u16(110, 3)
u16(112, 1)
u16(114, 4)
u16(116, 7)
u16(118, 2)
u16(120, 4)

target = Path(__file__).parent / "corpus" / "gsub" / "basic.bin"
target.parent.mkdir(parents=True, exist_ok=True)
target.write_bytes(data)
print(target)
