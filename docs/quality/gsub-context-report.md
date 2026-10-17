# Contextual OpenType substitutions — 2026-10-17

Source: `80f0246ec0bb75627a910a8054fb33957be4fedd`. This extends the
bounded shaping subset; it does not complete script shaping or release acceptance.

GSUB 5/6 now parse glyph, class and coverage formats, nested single/ligature/
contextual actions and extension 7. `calt` participates by default. Matching
finishes before action execution. Action indices follow earlier contractions;
backtrack is nearest first and lookahead is not consumed. Parent and nested
lookup flags remain independent. Current first glyphs are tested by Coverage
even when their GDEF class is ignored for subsequent matching.

Linked stable slots avoid moving the complete suffix for each nested contraction.
Removed slots retain forward links so resume fences survive deletion. Mark
component provenance survives contraction and unary replacement. Whole reachable
lookup data is validated before mutation; unsupported dependencies skip the root.
Public error outputs remain unchanged. Limits: 64 sequence positions/actions/
subtables, recursion 32 and the shared 4,000,000-operation budget.

## Executed checks

- The new public regression covers all six formats, ordered actions, distinct
  class definitions, Class 0, extensions, first-match ignore rules, GDEF filtering,
  lookahead/resume fences, nested ligatures, mark provenance, cycles and invalid
  records. It rejects 4,740 truncated tables without changing public outputs and
  checks glyph/source positions for 10,000 contractions.
- Windows static/shared and Linux ASan/UBSan/LSan each pass the three targeted
  context/GSUB/mark suites. The compiled old GSUB from `b7651a3` fails the new
  context expectation. Its installed-Calibri probe returns `[4,501]`, rejected
  by the table-derived `[4,502]` expectation; the new engine returns `[4,502]`.
- Optional FontTools 4.67.0 independently decompiles all six synthetic fixtures
  on Windows/Linux. Calibri 6.27 active `ccmp` root 5 calls nested lookup 162,
  replacing acute glyph 501 with 502 after A. This is a constrained table oracle,
  not a full reference shaper. No installed fonts are redistributed.
- Arial 7.06, Segoe UI 5.72 and Calibri 6.27 pass 36 sample executions and 18
  independent anchor comparisons across Windows static/shared and Linux
  sanitizers. Stacked/ligature samples are smoke checks, not anchor conformance.
- Fresh GSUB libFuzzer campaign: 50,000 runs, seed 181018, seven initial seeds,
  max length 4096, 22.027 seconds, 1,861 counters, exit 0. ASan/UBSan active;
  LSan disabled only for the documented Alpine libFuzzer driver allocation.
  Targeted Linux CTests retain leak detection.

[Native CI run](https://github.com/Lulus792/NexaUI/actions/runs/39064946893)
passed all seven required jobs: Windows 101/101 and macOS 88/88 in each
static/shared variant; Ubuntu static/shared and sanitizers each execute 102
successes with seven unavailable-Wayland skips (109 registered). Optional Windows
ASan and fuzz jobs were skipped; the fuzz result above is a separate local run.
Native CI does not establish physical input/display/screenreader acceptance.

Receipts: [CI](gsub-context-ci.json), [context oracle](gsub-context-fonttools.json),
[real fonts](gsub-context-real-fonts.json), [fuzz](gsub-context-fuzz.json).
Raw local evidence remains under `out/gsub-context-*`; compiled negative source
copies and failure logs are retained. See [cleanup](storage-cleanup-2026-10-17.md).

## Remaining scope

GSUB 2/3/8, contextual/cursive GPOS, Arabic joining and script reordering, complete
variable fonts, full IME document/reconversion, accessibility roles/text and
physical/release gates remain open. No universal speedup is claimed.

Contracts checked against OpenType 1.9.1 primary documentation:
[GSUB](https://learn.microsoft.com/en-us/typography/opentype/spec/gsub) and
[common layout tables](https://learn.microsoft.com/en-us/typography/opentype/spec/chapter2).
