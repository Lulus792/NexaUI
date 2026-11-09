# Font metadata for document presentation

SDK0.1.31 Development, code candidate
`9f0b2294d04d7f5fdd63d3fe5c118b2bd222eade`, 9 November 2026.
API implementation `c6f6d3aa05286fd8f476f17331ac5e7c3f5352ef`;
fixture cleanup `734efe5aaa53923fd903dcc9edb6773fd30feeae`;
package version correction `2e18de4a5eb6d1bbfbb1c1bf9d59a4046bbc43b5`.
All103 catalogue requirements, thirteen surface groups and remaining packets6–8
remain in scope. This is the reusable metadata prerequisite for original Editor
attributes, not a claim that those attributes have been adopted by the owner.

## Public contract and reuse

The existing SFNT table reader now supplies two optional queries:

- `nx_font_name_utf8` decodes a requested name ID into caller-owned, NUL-terminated
  UTF-8. NULL/0 queries the required length, excluding the terminator. Capacity
  errors report that length without modifying the output buffer. Selected
  malformed UTF-16, NUL scalars and invalid table offsets are rejected atomically.
- `nx_font_get_style` reads OS/2 weight1–1000, width1–9 and upright/italic/oblique
  metadata into a small value structure. Missing/unknown data is unsupported;
  truncated known tables and invalid ranges are errors, with cleared output.

Names support table versions0/1, Unicode encodings0–4 and Windows0/1/10.
Preference is deterministic: Windows English-US, Unicode language0, then the
lowest language/platform/encoding key, retaining the first equal record.
Version1 language-tag arrays and referenced storage are bounded; language-tag
text is not interpreted as locale negotiation. Unsupported Macintosh/codepage
records do not become an inferred name. Request typographic family16, then1
only when unsupported; typographic subfamily uses17/2 similarly.

Work is bounded to4096 name/language-tag records and4096 UTF-8 output bytes.
The queries allocate nothing and retain no additional font state. OS/2 versions
0–5 are supported, including shortened68-byte legacy version0; oblique bit9 is
used only for versions4/5. These are static-face metadata, not synthetic styling,
variable-instance application, font authenticity or a coverage guarantee.

The [public text contract](../concepts/text.md) follows the primary
[OpenType name table](https://learn.microsoft.com/en-us/typography/opentype/spec/name)
and [OS/2 table](https://learn.microsoft.com/en-us/typography/opentype/spec/os2)
specifications. Existing font opening/rendering/metrics implementation bytes
are unchanged after removing the added queries. All previous public declarations
remain; the version advances0.1.30→0.1.31. The installed Linux shared library
retains all539 prior `nx_` definitions and adds exactly the two query functions.

## Executed evidence

The [source binding](font-metadata-source-binding.json) verifies1086 exact Git
inputs and all316 compiled SDK/test sources; three consumer objects bring the
total to319 explicit-C17 objects. Generated protocol sources are recorded
separately. Three installed profiles use Release static, Release shared and
whole-library ASan/UBSan/LeakSanitizer respectively, with warnings as errors and
C extensions disabled throughout the library/test/consumer graph.

Each profile passes37 selected CTests:111 actual tests, including twelve private
AT-SPI tasks in total. This includes metadata, existing font/cmap/outline/raster/
cache/collection/shaping/GSUB/GPOS/layout tests, document presentation, editing
and clipboard regressions. Each installed external package consumer additionally
opens a synthetic SFNT face through installed headers/libraries and checks
positive UTF-8 name, weight, width and italic results; all three executions pass.
The native tasks retain their own compositor/provider guards, but do not yet
exercise the new attributes on the actual original Editor.

Metadata cases cover exact Greek/CJK/supplementary UTF-8, deterministic record
selection/reordering, name IDs16/1, capacity boundaries with unchanged buffers,
invalid offsets/truncation/surrogates/NUL, version1 tag references,4096-record/
4096-byte limits, unsupported encodings/tables, empty names, every shorter
known OS/2 table length, invalid weights/widths, legacy versions and800 explicit
bit mutations. Four actual borrowed Candara files (regular, bold, italic and
bold italic) pass in every profile: twelve real-font runs with exact expected
family/weight/slant. Font bytes remain external; their provenance/hashes are
recorded without redistribution. These measurements prove metadata parsing,
not full glyph coverage or rendering acceptance for those fonts.

Three supplemental [original-owner font checks](font-metadata-owner-font.json)
read the actual Liberation Mono file used by the original Editor. Its SHA256
matches the earlier native runtime provenance; all three current SDK probes
report `Liberation Mono`, weight400, width5 and upright. These three runs are
separate from the twelve Candara runs in the raw SDK ledger. The borrowed font
bytes are excluded from both evidence archives. Native attribute publication
by the original owner remains the next step.

Two separately compiled defects change English-name priority and the weight
field offset. The same final test fails each at its intended assertion with
ASan/UBSan/LeakSanitizer still enabled and without a sanitizer diagnostic.
The [raw ledger](font-metadata-local.json), [exact commands](font-metadata-checks.json)
and [verified archive ledger](font-metadata-archive.json) retain the actual
binaries, sources, logs, package consumers and failure history.

## Findings retained and next owner step

The first deliberate defect exposed a96-byte abandoned test fixture on early
assertion return; explicit fixture cleanup fixes that test path. This is not
suppressed or reclassified as a successful clean initial control. The first
matrix driver also used a pluralized GPOS test target and stopped before build;
the actual existing target is retained in the corrected driver.

An initial package consumer returns16 because CMake said0.1.31 while the public
header still said0.1.30. Both are now0.1.31 and all final installed consumers pass.
The initial Linux CI also detects that mismatch in four actual AT-SPI toolkit
version queries; its four failures and exit8 remain in the original complete log.
The extended Clang22/Werror build also rejects an ambiguous adjacent-literal
spelling in the existing Unicode layout-work fixture. Explicit parentheses
preserve the exact intended string and all compiler diagnostics; no warning,
sanitizer or test is disabled. Initial source exports, logs and private builds
remain archived rather than overwritten by passing attempts.

All seven required exact-source jobs pass in
[run42613670317](https://github.com/Lulus792/NexaUI/actions/runs/42613670317):
Windows146/145, macOS122/121, Linux268/266, a separate one-test Linux repetition,
and sanitizer268 in each driver-discovery order. No CTest skips occur. Installed,
close-request and add_subdirectory consumers pass in six platform jobs. Optional
Windows-ASan/fuzz jobs skip; no new successful fuzz campaign is claimed.
[Complete decoded logs, full head-SHA/job metadata and historical failures](font-metadata-ci.json)
remain source-bound; these root CI results do not establish original Editor
attribute adoption.
Normal application adoption, native Editor attribute defaults/runs and changes,
selection colors, unit-correct text size, broader native interfaces, AX/UIA,
real installed IMEs/screenreaders, full font/fallback coverage and physical/human/
design/performance gates remain open. Original Editor geometry evidence stays
at its [own source revision](editor-presentation-report.md).

The independently read Aetheri HEAD and status of all53 frozen original inputs
are recorded in the source binding. No active Aetheri source, pin, default or
build was modified. This development snapshot does not authorize a1.0 release.
