# Copied application document geometry and text attributes

SDK0.1.30 development, source `1ee9d99540587494a0667e2c41caf63e6fd27e14`,
9 November2026. Initial implementation `c1221f05d11feb7511163876d3ab7fc41d1349ee`;
the fixed source is the acceptance candidate. This is a packet6 SDK prerequisite,
not original Editor layout adoption or completion of the103-ID catalogue.

## Existing ownership and additive entry

`nx_text_document_set_presentation` attaches optional copied geometry/attributes
after either existing document registration function. It checks the current
document identity/revision, including the returned state of an admitted edit.
The application owns the actual layout, drawing, model/history and input.
No model/layout/font pointer or callback survives the call. Native queries read
an independently owned completed snapshot and never call application code.

The [contract](../concepts/document-presentation.md) defines scalar-boundary UTF-8
byte ranges, shared cluster/ligature boxes, explicit zero-length caret boxes,
logical window coordinates, sparse coverage, default attributes and overriding
nonoverlapping runs. Missing geometry remains NotSupported. This permits bounded
viewport geometry without claiming positions for the complete8-MiB document.
It does not add scrolling, derive advances or substitute a fixed-cell layout.

Per document, limits are65536 boxes,4096 runs,32 attributes per set and8 MiB of
complete owned presentation. Validation/staging precedes the context allocator;
failure permits retry without attaching partial presentation. Linux's existing
32-MiB snapshot limit charges the copied data and alignment. Native allocation
failure retains the previous publication.

Linux AT-SPI adds character/range extents, point-to-offset and default/run/named
attribute queries through the existing Text interface. Native scalar indices
are converted separately from byte offsets. WINDOW/PARENT translation and
rounding reuse Component geometry; unknown SCREEN origin remains NotSupported.
Point queries preserve ancestor scroll clipping. Changed style/geometry emits
TextAttributesChanged/VisibleDataChanged; identical publication emits neither.
The [wire contract](https://raw.githubusercontent.com/GNOME/at-spi2-core/main/xml/Text.xml)
and [attribute-run semantics](https://gnome.pages.gitlab.gnome.org/at-spi2-core/libatspi/method.Text.get_attribute_run.html)
were checked against the implementation.

## Exact-source local matrix

1082 exact Git blobs are exported without editing the frozen source. GCC15.2
Release static/shared and Clang22.1.3 Debug ASan/UBSan run on Alpine/WSL2,
kernel6.18.33.2. The actual native transport uses libatspi2.60.5, an owned private
D-Bus/registry and the existing private Weston14.0.2/Xvfb fixture. No shared
desktop bus, active Aetheri process or global configuration is changed.

| Profile | Selected CTests | Private native tasks | Installed consumers | Compiled objects |
| --- | ---: | ---: | ---: | ---: |
| Release static | 23 | 4 | 1 | 92 |
| Release shared | 23 | 4 | 1 | 92 |
| Debug ASan/UBSan | 23 | 4 | 1 | 92 |

Each92 includes91 SDK/test/peer objects and one external package-consumer object.
The matrix totals69 actually executed CTests,12 native tasks,3 installed consumers
and276 objects. Four native tasks per profile cover existing document edits,
cache budget and memory-clipboard regression plus the new presentation peer.
The library and new fixtures explicitly compile as C17 with warnings as errors.
Inherited older fixtures/package consumer use the compiler-selected language
default satisfying exported `c_std_17`; the receipt records those exact commands
and default-standard macros instead of claiming an explicit C17 flag everywhere.
The sanitizer profile instruments all selected SDK/peer/consumer objects and
retains leak detection, frame/time limits and existing rendering behavior.

Portable cases cover copied cluster/bidi-order rectangles, explicit caret,
range union, hit/miss, defaults/overrides, identity/revision/duplicate attachment,
malformed ranges/UTF-8/NUL/nonfinite geometry, limits, omission,8-MiB sparse text
and allocation-failure retry. Original source arrays are overwritten before
querying their copies. The separate libatspi peer links no NexaUI/model code;
it checks scalar WINDOW/PARENT geometry, range/caret extents, hit testing,
defaults versus overrides, replacement/omission, events/idempotence, actual
8-MiB text with deliberately limited geometry, stale references, unavailable
SCREEN coordinates, detach and exit0. This fixture declares synthetic rectangles;
it is not a rendered text/glyph or original-model geometry proof.

A separately compiled control changes only the private native presentation
accessor to return no data. The same final independent peer fails at its first
character-extents assertion. That is an actual missing-presentation control,
not a claim of a production defect. No installed SDK or original app is replaced.

## Portability, compatibility and CI

Initial c1221f0 CI exposes a real Windows portability error: `max_align_t` is
unavailable in the selected MSVC C17 headers. Fixed1ee9d99 aligns a union of the
actual stored types. Alignment invariants and both selected tests pass in a
local MSVC19.38 Shared build; the complete Windows CI then passes in both modes.
Both original failing logs remain retained; no test or compiler setting is weakened.
The earlier native-client trial assumed a GLib error for missing defaults;
the final peer asserts the actual D-Bus NotSupported reply directly.

Removing only the additive public type/function sections and restoring version29
reproduces the previous public header byte-for-byte. All530 prior same-platform
SDK0.1.29 `nx_` ELF definitions remain. Nine definitions are added: one public
entry and eight private helpers, which are not new public API promises.

All seven required exact-source jobs pass in
[run42582758910](https://github.com/Lulus792/NexaUI/actions/runs/42582758910):
Windows145/144, macOS121/120, Linux267/265 and sanitizer267 in each driver order.
The static Linux job also passes its separate one-test repetition. There are no
CTest skips; two optional Windows-ASan/fuzz jobs skip. Installed, close-request
and add_subdirectory consumers pass in all six platform jobs. The new native
presentation case is required by the Wayland-result checker.

[Raw local evidence](document-presentation-local.json),
[Git/header/symbol binding](document-presentation-source-binding.json),
[checks](document-presentation-checks.json),
[CI logs and exact head SHA](document-presentation-ci.json) and
[archive ledger](document-presentation-archive.json) retain source, flags,
commands, objects, packages, native outcomes and the compiled control. The raw
archive verifies3254 files and four links, including the local MSVC guard.

## Next owner work and remaining gates

Next connect actual original Editor layouts/carets and truthful font/style data
to this entry. Reuse its existing layout/selection APIs; do not estimate uniform
advances for proportional, combining, ligature or bidi text. Verify scrolling,
selection/edit/IME/review transitions, density/style changes, retained references
and native close against that real owner. This SDK fixture does not establish
those workflows or full offscreen geometry/scroll-substring support.

Windows/macOS native document presentation, broader text interfaces, installed
IMEs/screenreaders, glyph fallback, physical/human/design/performance and normal
Aetheri adoption remain open. No active Aetheri source, pin or build changes.
All103 IDs and remaining packet6/packets7–8 persist. This is no1.0 release.
