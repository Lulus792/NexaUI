# Text stress and fuzz campaign — 2026-10-14

## Environment and method

- Windows 11 host with Intel Xeon W-2123, Alpine 3.24.2 under WSL, Clang
  22.1.3 for fuzzers, GCC 15.2.0 for the Release stress benchmark.
- Four isolated libFuzzer targets built with AddressSanitizer and
  UndefinedBehaviorSanitizer. Each started from a copy of its repository
  corpus, used random seed `1800`, 10,000 runs, `-max_len=512` and
  `-timeout=10`. UndefinedBehaviorSanitizer used `halt_on_error=1` in a
  confirming 10,000-run campaign. Generated discoveries were removed from
  the source corpora; the recorded coverage runs used copies under
  `/root/nexaui-fuzz-run`.
- LeakSanitizer was disabled with `ASAN_OPTIONS=detect_leaks=0` because this
  Alpine libFuzzer exits with a 56-byte allocation attributed by `addr2line`
  to `fuzzer::FuzzerDriver`, even for the empty input. This campaign does not
  establish leak freedom.

| Target | Initial seeds | Runs | Final coverage counters | Sanitizer result |
| --- | ---: | ---: | ---: | --- |
| SFNT directory, cmap, metrics, outlines, raster/cache | 1 | 10,000 | 840 | no finding |
| GSUB table | 1 | 10,000 | 637 | no finding |
| GPOS table | 1 | 10,000 | 624 | no finding |
| UTF-8, boundaries, line breaks and text edits | 11 | 10,000 | 965 | no finding |

The counters are libFuzzer instrumented edge coverage, not percentages or
proof of correctness. Font targets mutate a 512-byte SFNT or a layout table
inside a synthetic face. The text target rejects inputs over 512 bytes to
bound each iteration. The SFNT seed is generated from the existing synthetic
font fixture (`SHA-256 42fcdb3d3f29a9f4e6674db7dabef68a55943c02760c25fce77b2522b84c43cd`).

## Script presence in text seeds

`python3 tools/report_script_coverage.py` maps the valid UTF-8 seed scalars
to pinned Unicode 18.0.0 `Scripts.txt`. **21 of 175 assigned scripts** occur:
Arabic, Armenian, Bengali, Cyrillic, Devanagari, Ethiopic, Georgian, Greek,
Han, Hangul, Hebrew, Hiragana, Katakana, Khmer, Lao, Latin, Myanmar, Tamil,
Telugu, Thai and Tibetan. One seed is malformed UTF-8. Common and Inherited
characters are excluded from that count. Presence in the seed corpus is not
shaping support or visual correctness.

## Edit stress

`nexaui_text_stress_bench` uses repeated mixed combining, Han, emoji and
Arabic text. Each of five Release runs creates a fresh buffer and measures 32
insert/delete pairs at the beginning, then checks that the original bytes
are restored. Times include movement to document start and full boundary
rebuilding; they exclude initial buffer creation. One local run yielded:

| Text bytes | Min | Median | Max |
| ---: | ---: | ---: | ---: |
| 4,114 | 0.544 ms | 0.680 ms | 0.898 ms |
| 32,780 | 4.433 ms | 4.478 ms | 4.702 ms |
| 131,076 | 17.968 ms | 18.455 ms | 19.428 ms |

Values are milliseconds per insert/delete pair, not frame times. The
growth is consistent with the buffer's documented O(text length) edit path.
The five samples are a narrow local baseline, not a latency guarantee.

## Known gaps

- The seed corpus has no examples for 154 assigned scripts. The report script
  prints their names. Complex shaping, mark attachment, fallback quality and
  native font coverage need separate script-specific tests.
- The first campaign did not cover text layout. A follow-up campaign below
  adds it. Native bidi rendering, font fallback, CFF outlines and large font
  files remain outside these corpora. The SFNT seed is synthetic.
- The stress benchmark edits only at the start of a contiguous buffer and
  excludes concurrent UI/render work. Large documents have high edit cost;
  scalable storage remains a planned architecture task.
- Installed IME behavior and platform text rendering remain outside this
  campaign.

## Follow-up layout campaign — 2026-10-14

`nexaui_layout_fuzz` reuses the 11 text seeds and adds six dedicated
wrapping, hard-break, paragraph-separator, bidi, mark and whitespace cases.
Deterministic Clang 22.1.3 ASan/UBSan runs used
`-seed=1800 -runs=10000 -max_len=256 -timeout=10`, with UBSan
`halt_on_error=1` and LeakSanitizer disabled for the Alpine runtime issue
above. A 16-seed post-fix run reached 3,217 instrumented coverage counters;
the final 17-seed corpus also completed 10,000 runs with exit code 0 and no
sanitizer finding. The target exercises three layouts per
valid input: automatic direction at 24 px width, forced LTR at 96 px, and
forced RTL at 240 px, all at 20 pixels per em.

The initial run found a real contract defect: U+001C–U+001E have Unicode
bidi class B but line-break class CM. The layout did not split there, so its
single-paragraph bidi analysis rejected valid UTF-8 containing one of these
separators before the paragraph's end. The 2-byte minimized input U+001E,
U+0001 reproduced the failure. Layout now treats bidi class B as a hard
paragraph boundary; the minimized input and a regression covering all three
separators pass. Layout fuzzing still uses a synthetic font and at most
256-byte inputs; it does not verify visual glyph quality or large documents.

The rebuilt Alpine/GCC suite passed 71/71 tests under a private Xvfb display.
The first run on the existing display failed only because X11 authorization
was unavailable. Windows/MSVC Debug text-layout tests passed in both static
and shared builds (1/1 each).
