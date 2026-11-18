# External CPU frame handoff

Standalone consumer `5fa854d9b0f68c7951ef418801fb846accd248bb` starts the
packet7 content seam using existing installed NexaUI 0.1.33/source
`368cd38292498bd538964ba514ff02e0e9924358`. It adds no installed SDK API,
engine dependency, Aetheri implementation or application-pin change.
[Consumer and build/run instructions](../../examples/38_external_content/README.md)

## Existing contracts and the missing consumer boundary

`nx_snapshot_exchange` transfers immutable producer-owned snapshots between a
producer and UI consumer; its borrow must end before the UI frame ends.
`nx_viewport` instead leases image pixels through rendering after `nx_end_frame`.
Directly retaining the snapshot borrow or returning its later-freed pixels
cannot satisfy both contracts. Extending the snapshot borrow would change its
public lifetime rule. This consumer instead copies each new accepted frame into
an explicitly bounded staging buffer and releases the snapshot inside acquire.
The ordinary viewport lease then protects staging through rendering.

The app supplies a fresh content generation after session/tab replacement.
Viewport resize records an increasing request ID and requested physical size.
The producer echoes that tuple and a monotonic frame sequence in an owned
in-process header/pixel allocation. It uses existing snapshot publication and
disposal, not another thread/channel implementation. The consumer validates
generation, resize, sequence, dimensions, RGBA8 format, stride, payload length
and budget before copying. Equal sequences reuse the same immutable staged
pixels; lower sequences and mismatched requests are rejected.

Allocation/budget failure preserves the previous request and staging. Transition
to zero-size suspension retires staging and advances the request. Destroy while rendered
pixels are leased fails and leaves the object usable for completion. The app
retains the exchange and worker lifetime, so an in-flight job can finish after
the old surface and context close; the replacement generation rejects its frame.
The 64MiB maximum is this example's explicit accepted-payload/staging policy,
not a global SDK limit or a bound on arbitrary upstream engine allocations.

## Executed evidence

Clang22.1.3 on Alpine-Dev/WSL2, uid65534, passes **12 installed consumer CTests**:
four each in static Release, shared Release and Debug `-O1` ASan/UBSan/LSan.
C17/extensions OFF and conversion/shadow/Werror remain enabled. Sanitized
positives use leak detection and no suppressions. The shared executable resolves
the selected installed SDK; all three SDK library hashes match the prior
source-bound package receipt. No fonts or content engines are needed by these
headless compositions.

| Case | Observed behavior |
| --- | --- |
| Flow | Padded producer rows, unchanged-frame reuse, scale resize, suspension, old-request rejection and budget failure/recovery |
| Malformed | Truncated header/payload, wrong generation/request/dimensions/format, zero sequence/stride, excessive stride and decreasing frame sequence |
| Lifetime | Resize allocation failure preserves request, active-render destroy refuses, completion/close succeeds, replacement rejects the old generation |
| Worker | A real OS thread prepares an old request, waits through surface/context close and replacement, then publishes; old content is rejected and fresh content renders |

Rendered cases compare all 16,384 bytes of a 64x64 RGBA composition against a
separately built reference, including the ordinary host button and content
rectangle. Tests reacquire the exchange before end_frame to prove the snapshot
borrow already ended. They also publish another producer frame, disposing the
old allocation before rendering the prior command stream, to verify staging
remains readable. All tracked surface/viewport allocations balance at teardown.
Unchanged-frame tests observe no additional staging allocation/copy; these are
work counters, not performance or input-latency measurements.

Three private compiled counterexamples fail at the intended boundary:

- Removing the generation check admits an actual worker result for the closed
  tab and fails the expected stale-frame rejection.
- Retaining the snapshot borrow fails the independent second acquire before
  UI end_frame.
- Returning producer pixels after releasing their snapshot produces a real
  ASan heap-use-after-free when that allocation is retired before rendering.

The last diagnostic is a deliberately broken consumer, not an SDK regression.
The test source is unchanged in all negative copies. All configure/build steps
pass; each probe exits1 at its expected check, within the existing 15-second
test budget. No outer timeout is accepted as a diagnostic result.
[Source/object/command/test/negative receipt](external-content-local.json)
and [independent archive readback](external-content-final.json).

The first compiler pilot rejected a tautological 64-bit size comparison. The
corrected code checks viewport dimension limits and divides the explicit byte
budget before multiplication. A later build-helper preflight accidentally
renamed a hash function and failed before configure; corrected targeted naming,
both pilots and the intermediate strict profile results are retained. A worker
join review also prevents the unexecuted Windows test branch from freeing state
under a still-live worker on join failure. Windows/macOS consumer builds remain
unexecuted acceptance gates.

`nexaui-content264.tar.gz` contains 108 regular members, 8,618,463 bytes,
SHA-256 `a455aa25db854189a9a9d3d6164e908fee7b9dd534a3f13cf4fb6c65448c9b71`.
It retains the exact consumer/catalogue inputs, compiled objects/binaries,
complete logs, pilot preimages, negative source and helpers. Every member was
read back and hashed twice. The SDK implementation remains `368cd382`, whose
seven required [platform/package CI jobs](https://github.com/Lulus792/NexaUI/actions/runs/43760877115)
are separately source-bound. That SDK CI run does not build this new consumer.

## Remaining packet7 acceptance

This section records `5fa854d`. The
[native follow-up](external-content-native-report.md) adds ordinary-window,
worker-fd, host-focus and independent displayed-pixel evidence at `2ef9a56`.
Selected-engine, guest-input, IME and engine-accessibility gates remain open.

This advances the CPU image/lifetime boundary for R06/R10/D08/P05/Q02/Q08.
It does not establish a maintained browser/PDF/guest engine, native content
presentation, input/host escape, stuck-key release, IME/candidate geometry,
engine accessibility subtree, crash recovery, GPU import or physical/human/
performance acceptance. Those remain concrete integration work and named engine/
host gates. Physim and NexaCloud retain their separate real-app adoption gates.
The fixture does not decode documents or execute guest/service operations;
producer pixels never become host permission authority. The full 103-ID catalogue
and remaining packets stay active. Aetheri can independently adopt this bounded
CPU handoff where its own producer and lifecycle policy satisfy the contract.

## PNG producer checkpoint, 2026-11-17

Consumer `564e5e980e633b3a16ecbd3c957073746cdb2ccf` uses installed SDK
`b53513b390dbf135050a858fbdfd434edc04ad08` and Alpine libpng `1.6.59-r0`.
The optional producer uses the [versioned libpng read API](https://raw.githubusercontent.com/pnggroup/libpng/v1.6.59/png.h)
to decode borrowed PNG bytes into straight-alpha RGBA8. It fits that image to
the requested physical size with transparent margins, then transfers the
complete frame through the existing exchange and viewport. Unit coverage
includes alpha, centred fit and reduction pixels, truncated data, size limits
and reuse after rejection. A compiled variant that admits a 4096-pixel source
fails the source-dimension assertion.

The native fixture decodes four valid PNGs and rejects one truncated PNG
before publication. It exercises old-generation rejection, accessible error
status, retained image and semantic object, recovery, 2× backing and close.
In all three profiles the malformed input leaves every one of the 93,184
displayed image pixels intact. Recovery updates 5,824 pixels, with 23,296
bytes staged through the existing damage contract. The worker reports five
completions, four publications and four disposals on normal close.

On Alpine with Clang 22.1.3, 43 installed CTests and sixteen private native
cases passed. These include static Release, shared Release and ASan/UBSan/LSan
with PNG enabled, and a default build without libpng linkage. The standalone
host's dynamic link audit records `libpng16.so.16` only with the option on.
Archive `nexaui-content-png384.tar.gz` binds exact source, commands, fixture
PNGs, logs, captures and the counterexample. Readback verified SHA-256
`81c7ac570c5ab8ced3e8f1ed699efbf366b7f71945839f62a1c60c5dd7eb5cdf`.
Local receipt `content-png384-final.json` records the library/header and
member hashes.

Production image viewer navigation, process-level decoder isolation, broader
format and metadata handling, PDF/browser/guest engines and GPU handoff
remain separate acceptance work.

## PDF producer checkpoint, 2026-11-17

Consumer `623d1cba50d198ede2788ee190ff27b5813477ae` uses the installed SDK
`b53513b390dbf135050a858fbdfd434edc04ad08`. The optional Linux worker runs
Alpine `pdftoppm` 25.12.0-r1 in a separate process. The fixture uses verified
packages extracted to a private test directory. The host passes an already
open regular file and explicit page as input, reads bounded PNG output, and
uses the existing owned CPU snapshot and viewport lease. The native host's
dynamic link audit contains no Poppler library. [Poppler package](https://pkgs.alpinelinux.org/packages?branch=v3.24&name=poppler-utils)

The independent native peer selects page 1, triggers an invalid PDF render,
queries the error status and checks that the image and its native semantic
object remain available. When it selects **Next page**, every pixel matches
the expected page 2 image, including the 5,824 changed pixels. It repeats
the page render with 896×416 backing. **Previous page** restores the original
page 1 pixels. The native workflow rejects a late old-generation page after
replacement and accounts for six completions, five publications and five
disposals after close. Both page images match across all three build profiles.

The process test checks a stalled child, oversized stdout and a nonzero exit.
It verifies that failed calls preserve output parameters and leave no child
to reap. The real renderer is exercised separately by the native test.
On Alpine with Clang 22.1.3, 46 installed CTests and nineteen private Wayland
cases passed across static Release, shared Release, ASan/UBSan/LSan and the
default build. The exact renderer SHA-256 is
`e969e55d7e2229f1e438284f9e8cfd666313e45a9b7b6265ccbf8b4ade5f2070`.

Archive `nexaui-content-pdf387.tar.gz` binds source, commands, fixture PDFs,
captures, package checksums and logs. Readback verified SHA-256
`836ab956d9b4edeb61c9e75f83f37ab7cde6a94a5ef5a0b4de9f98fee95a5c33`.
Local receipt `content-pdf387-final.json` lists its member hashes.

Production viewer integration, stronger process isolation, semantic document
text, links, search, selection and GPU/physical acceptance remain open.

## PDF text checkpoint, 2026-11-17

Consumer `0e94bb21266c8825fa4e21baf8418611c21e16a9` uses installed SDK
`b53513b390dbf135050a858fbdfd434edc04ad08` and Alpine Poppler tools
25.12.0-r1. The worker passes the same open immutable file and page number
to `pdftoppm` and `pdftotext`. It bounds raw UTF-8 text at 32 KiB and appends
it to the existing owned pixel snapshot. Image and text share the source
generation, resize and sequence. The viewport stages and copies both before
publishing its read-only AT-SPI text child. Empty extraction produces a
page image with the host controls. [Poppler upstream](https://poppler.freedesktop.org/)

The fixture adds an invisible PDF text layer with non-ASCII characters. The
independent peer reads both full UTF-8 page strings and checks AT-SPI scalar
counts, absence of caret and selection, and read-only state. It verifies
that failed rendering keeps the old text, a late old document adds no text,
and old text objects become Defunct after page/resize changes. Navigating
back restores the original page text. The displayed image pixels equal the
previous PDF checkpoint. A compiled counterexample that always extracts
page 1 draws page 2 but fails the native text check.

The installed tests reject invalid UTF-8 and embedded NUL, preserve the
accepted pair on allocation failure, retry the same publication and clear
text when resizing rejects an old frame. All 50 installed CTests and nineteen
native cases passed across static Release, shared Release, ASan/UBSan/LSan
and the default build on Alpine with Clang 22.1.3. The `pdftotext` binary
SHA-256 is `04f1da090e97301791df5308a3612c73e48413d9d9b431ff9a00691cd2902330`.

Archive `nexaui-pdf-text391.tar.gz` contains exact source, PDFs, direct
Poppler output, captures, installed/native logs and the counterexample.
Readback verified SHA-256
`99f223bf7542439c4bbca976dccc219c6ffa22d2d71637c049dc60b14ab016ff`.
Local receipt `pdf-text391-final.json` lists the member hashes.

Production PDF viewer integration, text geometry, links, search, selection,
spoken screenreader output and engine sandboxing remain open.

SDK `85f5d1cab97ff4488062b80d0149dc4efddcb803` accepts owned presentation on
read-only snapshots through the existing `nx_text_document_set_presentation`
entry point. Revision 0 identifies a read-only snapshot. Twelve headless and
twelve native Wayland CTests passed across static, shared and sanitizer builds.
The independent AT-SPI peer verified shared word bounds, Unicode scalar hit
testing, unmapped whitespace, absence of a caret and read-only state. The
Linux shared build has the same 544 `nx_` exports as `b53513b`. The first
local native invocation omitted Xvfb. The corrected fixture ran the same
builds through the private Xvfb/Weston test environment.
[SDK platform CI](https://github.com/Lulus792/NexaUI/actions/runs/43988082890)

Local evidence archive `nexaui-readonly-geometry395.tar.gz` has SHA-256
`b03dae622d27c8a31b960ca342c3e28c26356f526169dea9c00b3df6de48e4d2`.

## PDF word geometry checkpoint, 2026-11-17

Consumer `30cf77aea466838b7a38b6fb86ca48f1b3c37b7a` uses installed SDK
`85f5d1cab97ff4488062b80d0149dc4efddcb803` and Poppler 25.12.0-r1.
It matches bounded TSV word rows against the complete plain UTF-8 text in
order and maps the page coordinates through the actual PNG fit. The image,
text and matching word boxes share the same generation, resize and
sequence. On a mismatch the snapshot retains the image and text.

The independent AT-SPI client checked both pages and backing scales.
`Résumé` yields the shared window rectangle (48, 181, 120, 30) and
`Grün` yields (48, 181, 72, 30). A point inside each second word
returns its first Unicode scalar offset. Spaces have no rectangle.
Failed rendering retains the previous page and bounds. Page changes and
resize retire the previous accessible text object. Deleting the presentation
boxes in a separate build retained the captured image and failed the native
geometry assertion.

All 50 installed CTests and 19 private Wayland runs passed across static
Release, shared Release, ASan/UBSan/LSan and the default build on Alpine.
Headless cases cover letterbox offsets, a TSV text mismatch and rejection
of a box that starts inside a UTF-8 scalar without changing the accepted
snapshot. Native pixel captures match the previous PDF baseline.

Local archive `nexaui-pdf-words398.tar.gz` contains the exact source,
fixture PDFs, raw text and TSV, captures, logs and counterexample. Readback
verified SHA-256
`e5fdd88194782bba54478438db262a31f38955052ff310238c93fd0b3cf68b79`.
Local receipt `pdf-words398-final.json` lists the member hashes.

Character-level geometry, richer document reading order, links, search and
selection remain open, as do production isolation and human screenreader
acceptance. A05 and D08 remain partial.

## PDF page search checkpoint, 2026-11-17

Consumer `76b756cbfc228d5d86dec73e7619837aa91503db` uses installed SDK
`85f5d1cab97ff4488062b80d0149dc4efddcb803` and Poppler 25.12.0-r1. The
example hosts a NexaUI text field with window IME and clipboard. It finds
literal case-sensitive UTF-8 occurrences in the accepted page text and
exposes the count as a status. Matches intersecting known word boxes use
the same logical coordinates as AT-SPI. The query persists across page
changes and backing-scale changes. The worker does no additional work on
a query.

The ordinary native peer verifies Ctrl+F, Escape to the page, Tab through
the search field, keyboard clear, AT-SPI editing and highlight readback.
`Résumé` gives one result on page 1, zero on page 2, and one after
returning to page 1 at 2×. The Weston simple-IME peer composes `ü` on
page 2. The preedit leaves the committed query empty. Committing it
returns one hit and marks `Grün`. An earlier protocol trace captured the
provider committing a stray `f` when focus moved before key release.
The shortcut now gives the field focus after the release. The simple-IME
peer moves focus and clears the field through AT-SPI. The provider
consumed Escape in a recorded trace, so keyboard exit with that provider
remains open.

Fifty installed CTests and 22 private Wayland runs passed. The native
matrix includes three Compose runs across static Release, shared Release
and ASan/UBSan/LSan. Captures of the unmarked pages match the earlier
baseline. The first-page highlight changes the same 3,570 logical pixels
at 1× and 2×. The composed second-page search changes 2,130 pixels within
the matching word. All profiles agree.

Local archive `nexaui-pdf-find406.tar.gz` contains the exact source,
fixtures, logs, captures and diagnostic protocol traces. Readback verified
SHA-256 `1133608ff462e47672e5a6cb16e1618f596ac4829998c42a1ee34521fda401a6`.
Local receipt `pdf-find406-final.json` lists the member hashes.

Document-wide search, normalization and case folding, character-level
geometry, links, selection and human screenreader acceptance remain open.
This advances A05/D08/R06 in the standalone consumer.

## PDF document search checkpoint, 2026-11-17

Consumer `abf98a5ed33cc4bd6a76f897b833fd94b01e7041` uses installed SDK
`85f5d1cab97ff4488062b80d0149dc4efddcb803` and Poppler 25.12.0-r1.
The worker pins the open immutable document across render requests. It
extracts the index after publishing the first page, with a three-second
tool deadline and limits of 256 pages and 512 KiB of extracted UTF-8.
The UI accepts ownership only for the active generation. It caches
the case-sensitive results for the current query and page. Page search
remains available if document indexing fails.

The independent Wayland peer observes a real late index from generation 1
being retired after replacement with generation 2. It checks the distinct
per-page and document totals, follows `Grün` to page 2 by keyboard and
wraps to `Résumé` on page 1 through AT-SPI. The keyboard jump focuses
the accepted page. AT-SPI activation while the query field has focus
keeps it ready for editing. The peer checks that the normal page
controls can take focus and that highlights occupy the expected word
bounds. Installed tests exercise an empty indexed page,
invalid UTF-8, missing page delimiters and both limits. They read an
already-open file after its original path was removed.

All 50 installed CTests and 22 private Wayland runs passed on Alpine with
Clang 22.1.3. The matrix includes static Release, shared Release, the
default configuration and ASan/UBSan/LSan. Three native runs include
Weston simple-IME. All three PDF profiles captured the same 2,130
highlight pixels on page 2 after the jump and 3,570 on the return.

A separate static Release probe counted matches in a synthetic index
of 256 pages and 512 KiB on a Xeon W-2123 under WSL2. Over 32 samples
per query, median thread CPU time was 2.69 ms for a one-byte query
with 524,032 matches and 2.17 ms for a 128-byte query with no matches.
The respective p95 times were 4.92 ms and 2.58 ms. The samples time
counting an in-memory index. They exclude extraction, frame layout and
presentation.

Local archive `nexaui-pdf-document416.tar.gz` contains exact source,
fixtures, logs, captures and the benchmark with its raw samples. Readback
verified SHA-256 `e70e44989c4d73328e483c9626dd9d7bfc2593a1338035cd4a952274a4a06bce`.
Local receipt `pdf-document416-final.json` lists the member hashes.

Search uses literal UTF-8 and navigates between matching pages. Unicode
normalization, case folding, navigation between occurrences on one page,
links, selection and human screenreader acceptance remain open.
This advances A05/D08/R06 in the standalone consumer.

## PDF internal links checkpoint, 2026-11-17

Consumer `a21d271a76af1c5e0410caf44a45062c42ae271a` uses installed SDK
`85f5d1cab97ff4488062b80d0149dc4efddcb803` and an optional Poppler-GLib
25.12.0-r1 child. ELF inspection shows that the child links Poppler-GLib.
The viewer passes the pinned file and one-based page number through the
existing bounded worker path. The PDF child exposes annotation mapping
and resolves direct and named GoTo destinations within the document.
The viewer validates the reply and maps its coordinates through the
actual fitted crop-box raster before staging links with the page frame.
[Poppler link mapping API](https://poppler.freedesktop.org/api/glib/poppler-Poppler-Page.html)

The independent Wayland peer checks the logical rectangle
`(352, 224, 84, 48)` at both backing scales. It follows the direct link
with Tab and Enter or a pointer click, and follows the named return link
through AT-SPI. The link on the retained page survives a failed render.
Links from replaced pages become Defunct. The input PDF contains an
external URI annotation. The peer observes only the supported in-document
link and page text under the viewport. The fixture exposes links as
buttons named for their destination.

A separate probe verifies a 90° rotated crop box. The actual Poppler
reply clips the link to `(14, 76, 12, 20)` in a 44×96 point page.
The installed consumer maps it into the fitted 448×208 image and checks
the transparent side margin. A headless child with an out-of-range
destination leaves valid image, text and word bounds intact.

Fifty installed CTests and 28 private Wayland runs passed on Alpine with
Clang 22.1.3, including six link workflows. Tests cover static Release,
shared Release, ASan/UBSan/LSan and the default configuration. Captured
page and search pixels match the previous baseline when link chrome is
idle. The dynamically linked viewer does not load Poppler libraries.

Local archive `nexaui-pdf-links420.tar.gz` binds source, package manifests,
fixture PDFs, raw link replies, logs, captures and the rotated probe.
Readback verified SHA-256
`73736744fb56b34fbd386839c0165bb5d19216e441448628dd741220d281cab2`.
The local receipt is `pdf-links420-final.json`.

External URLs, position and zoom destinations, a dedicated accessibility
link role, text selection and human screenreader acceptance remain open.

## PDF text selection checkpoint, 2026-11-17

Consumer `7c51f2c597a5a99243bf5fa89f205d4c08916eff` uses installed SDK
`85f5d1cab97ff4488062b80d0149dc4efddcb803`. The selection mode registers
the accepted immutable page with `nx_text_document_register_with_clipboard`
and read-only select/copy capabilities. Ordinary reading still exposes a
snapshot without caret. The model owns byte boundaries, selection revision,
pointer capture and the current page identity. A native request must match
the identity and revision before it can change the selection.

The independent AT-SPI and input peer selects `Ré` with Shift+Right and
`Résumé page` by dragging over the page image. It verifies that the third
word has no selection highlight. Independent `wl-paste` 2.2.1 reads
`Résumé page` after Ctrl+C and `page` after native AT-SPI CopyText.
The test waits for the viewer's copy status before reading the selection.
An AT-SPI write is rejected. The linked PDF keeps its link disabled during
selection. Page navigation clears the selection, retires the previous
native object and rejects a subsequent selection request to it. Escape
restores the ordinary snapshot on page 2.

Fifty installed CTests and 31 private Wayland runs passed with Clang 22.1.3
on Alpine. The three selection runs cover static Release, shared Release
and ASan/UBSan/LSan. Each capture changes 5,700 pixels within the two
word rectangles, apart from the selection mode's focus outline. The
page's image without selection matches the previous baseline.

Local archive `nexaui-pdf-select424.tar.gz` contains exact source, files,
commands, native logs and captures. Readback verified SHA-256
`fd233d0646d515e5ec4e8d7ab15e5599379458aa0a7d1c9d8e086ecebe1222ea`.
The receipt is `pdf-select424-final.json`.

The clipboard peer executable SHA-256 is
`7221ecacd258bf00989f30bff9ea02bc3cac17a588150f4bf81a8b0253ef9356`.
The fixture exercises selection within one page and highlights whole word
boxes. Cross-page selection, per-glyph highlight geometry, external
URL handling and human screenreader acceptance remain open.

## PDF external web links checkpoint, 2026-11-17

Consumer `3c6169e9eede74a431f6ec5af3f21ffe55ca87ce` uses installed SDK
`85f5d1cab97ff4488062b80d0149dc4efddcb803` and Poppler-GLib 25.12.0-r1
in the existing child process. The versioned child reply distinguishes
page destinations and HTTP(S) annotations. It admits up to 256 printable
ASCII bytes per address, requires a host, and rejects embedded credentials.
The frame validates the transport envelope before staging a reply.
[Poppler URI annotation API](https://poppler.freedesktop.org/api/glib/PopplerAction.html)

The viewer copies the address into a modal preview tied to the accepted
page identity. The preview exposes the full address to AT-SPI and uses
the measured text layout for display. Escape restores link focus.
An explicit **Open link** action submits the address to the host-selected
launcher on the existing worker. The child receives a single argument
and a private input descriptor, with the existing three-second deadline.
The UI reports the handoff result.

The independent native workflow checks the external annotation rectangle
at `(36, 224, 68, 48)` and the full address in the dialog. The page also
contains file, script-like and credential-bearing annotations, which do
not appear as active links. The peer checks that background Close is
blocked while the dialog is open. A separate `wl-paste` process reads
the exact query string and fragment after Copy address. Preview, Escape
and copy leave the local receiver untouched. **Open link** writes one
receipt, and the peer compares every byte and checks focus restoration
after closing the dialog.

All 50 installed CTests and 34 private Wayland runs passed with Clang
22.1.3 on Alpine. The three external link workflows cover static Release,
shared Release and ASan/UBSan/LSan. Each local receipt contains exactly
one copy of the whole address, including its original percent encoding.
The evidence includes the rendered dialog.

Local archive `nexaui-pdf-url427.tar.gz` binds exact source, fixture
PDFs, logs, screenshots and handoff receipts. Readback verified
SHA-256 `99c7d419e74a1b3e60fe1e2de616fc7719817d7b2800e75b8903c027af58a6b1`.
The local receipt is `pdf-url427-final.json`.

Product browser admission, internationalized URLs as submitted by the
document, page-internal position targets and human screenreader
acceptance remain open.

## PDF zoom and pan checkpoint, 2026-11-17

Consumer `c601d1f69d74c6561d1b022ab68e6335a9800528` uses installed SDK
`85f5d1cab97ff4488062b80d0149dc4efddcb803`. The app reuses the viewport's
clip and staged bitmap lease. For magnified frames it copies the command
stream into reusable storage and changes only the accepted bitmap's
destination rectangle. Keyboard and pointer pan use the same page
transform as link rectangles, accessible word bounds and hit testing,
search highlights and text selection. The allocator test rejects a failed
command copy, retries against the same lease and verifies the source
commands and pixels remain intact through rendering.

The independent private Wayland peer verifies 150% zoom, a 48-pixel
keyboard pan, a 40-pixel middle-button drag, clipped link and text
geometry, search highlighting and mouse selection of the word `page`.
Changing the backing from 448 × 208 to 896 × 416 preserves the logical
pan and word bounds and retires the old link. Fit restores the unzoomed
page. The worker records two staged copies before the backing change and
one fresh copy for the new backing. All four published frames are
disposed on close. The zoom controls and geometry are app-owned.

The 50 installed CTests and all 37 private Wayland runs passed with
Clang 22.1.3 on Alpine. The three zoom runs cover static Release, shared
Release and ASan/UBSan/LSan. The pixel audit matches the five unmarked zoom
captures against the expected raster and checks the exact unzoomed captures
against the preceding linked PDF baseline. Search and selection changed
4,708 and 4,477 interior page pixels respectively, excluding the cursor,
in each profile. The checked input path uses the accessible Zoom and Fit
actions, a keyboard arrow and a middle-button drag.

Local archive `nexaui-pdf-zoom430.tar.gz` binds the source, SDK receipt,
fixture PDFs, build and test logs, and native pixel captures. Readback
verified SHA-256
`144bbd65987de99daeaea4711b4393bb2b6fa0c6fed9e77011678817102bd387`.
The local receipt is `pdf-zoom430-final.json`.

Rendering fresh detail at higher zoom, thumbnail caching, photo-viewer
controls, physical-output checks and human screenreader acceptance
remain open.

## PDF page thumbnails checkpoint, 2026-11-18

Consumer `720368547e7c1908993b770bc6a6cb1269b76304` uses installed SDK
`85f5d1cab97ff4488062b80d0149dc4efddcb803`. The page dialog displays
three previews at a time with a selected state for the current page.
The same pinned Poppler file supplies smaller raster-only outputs on
the existing worker. Page renders take precedence over queued preview
jobs. One preview may be queued, running or awaiting adoption.

The UI owns fitted RGBA8 frames keyed by document generation, raster
size and a local revision. Changing group retires the previous group.
Closing and reopening the same group reuses its frames. An obsolete
completion is freed. The previous synchronous software render returns
before the UI drops a reference. At 2×, three 208 × 160 frames occupy
399,600 bytes in total, including their frame headers. Theme colors
belong to the surrounding controls and are recomputed on every draw.

All 53 installed CTests and 40 private Wayland runs passed with Clang
22.1.3 on Alpine. Each static Release, shared Release and ASan/UBSan/LSan
preview run opens a seven-page document, checks the current-page state and
the focus returned by Escape, reopens the same group, changes the backing
scale and visits the last group. Enter on the final card loads page 7.
The independent client checks the actual thumbnail pixels, the full
selected page pixels and retirement of retained native references.
Each run records exactly three 104 × 80 and seven 208 × 160 preview
results. The main exchange stages four frames, and every publication is
disposed at exit. The previous five page and placeholder captures are
byte-identical.

The cache CTest rejects delayed results after a document or group change
and after a 1× to 2× to 1× round trip. An isolated counterexample that
removes the raster revision check fails this test. Local archive
`nexaui-pdf-thumbnails432.tar.gz` binds exact source, SDK receipt, fixture
PDFs, commands, logs, captures and the counterexample. Readback verified
SHA-256 `790283d1721b17f470e936a6587e580b3bd13fa901b05587440eaf801c98dd8d`.
The local receipt is `pdf-thumbnails432-final.json`.

The dialog relies on a successfully indexed page count and uses an
in-memory CPU cache. PDF metadata UI, fresh detail rendering at higher
zoom, GPU-backed and theme-dependent caches, and physical and human
assistive acceptance remain open.

## PDF document properties checkpoint, 2026-11-18

Consumer `662de64567b216e18f477e51b05ce0e77719cb1e` uses installed SDK
`85f5d1cab97ff4488062b80d0149dc4efddcb803`. The Poppler-GLib 25.12.0-r1
helper reads the pinned document for title, author, subject, page count
and PDF version. It publishes a fixed, versioned reply with 512 UTF-8
bytes available per text field. The worker checks the envelope, numeric
fields and each string before passing it to the UI. The UI admits the
current generation only. The same helper retains its page-link mode.
[Poppler document API](https://poppler.freedesktop.org/api/glib/PopplerDocument.html)

**Pages → Properties** shows the document values. The existing modal stack
owns focus and dismissal. The existing scroll area updates caller-owned offsets.
The app publishes one read-only AT-SPI Text snapshot for the admitted
values. The seven-page thumbnail workflow remains independent of the
optional helper. A missing properties reply leaves the page, search and
any available thumbnails operable.

All 56 installed CTests and 43 private Wayland runs passed with Clang
22.1.3 on Alpine. The three properties workflows use static Release,
shared Release and ASan/UBSan/LSan. They reject the retired document's
reply, verify the current Unicode title and long author list, and display
an unavailable state for a 513-byte subject. Tab reaches the vertical
scrollbar and End reveals the last fields. AT-SPI reads the same complete
text before and after the scroll. The native peer verifies modal focus
and retirement of old text references. The properties captures change
12,365 interior pixels on scroll in each profile, and the five existing
page and placeholder captures stay byte-identical.

The transport CTest exercises ten replies, including undersized and
oversized payloads, invalid Unicode and invalid flags. Direct invocation
of the real child verifies both fixture documents in each profile. An
initial shared link run reached the fixture's frame limit. A dedicated
properties notification now triggers a redraw only when the dialog is
open, and the full rerun passes with the original limit.

Local archive `nexaui-pdf-properties435.tar.gz` binds exact source, SDK
receipt, fixture PDFs, raw child replies, commands, logs and captures.
Readback verified SHA-256
`7643732a395b69071356366eadd55f1fd3a1c047e27de9647fdebb18f147d010`.
The local receipt is `pdf-properties435-final.json`.

Additional metadata fields, fresh detail rendering at higher zoom and
human assistive acceptance remain open.

## PDF raster refinement checkpoint, 2026-11-18

Consumer `be15ffcc9eebfacca1f586515ed04b538e1f1db6` uses installed SDK
`85f5d1cab97ff4488062b80d0149dc4efddcb803`. Magnification initially draws
the accepted raster. The existing worker then asks Poppler for a higher
resolution when the fitted page's physical extent exceeds its current
detail. It reads the pinned PDF and keeps the same crop choice. The
requested long edge stays at or below 1024 pixels, and the decoder limits
an owned RGBA8 raster to 4 MiB.

The base frame supplies the exact fitted page rectangle. The app maps
the unpadded refinement to that rectangle using the existing viewport
clip and synchronous bitmap command. Only the bitmap and its destination
change. Metadata continues to use the accepted base frame's geometry and
identity. A refinement must match document generation, base resize,
base sequence, page and requested edge. A failed render preserves the
last usable raster. Pan reuses it and Fit releases it.

All 59 installed CTests and 46 private Wayland runs passed with Clang
22.1.3 on Alpine. Each static Release, shared Release and ASan/UBSan/LSan
refinement run holds a 672 × 312 result while changing the view to 200%.
The older result is discarded. Releasing the 896 × 416 render reveals
individual vector strokes without retiring the active link or changing
the AT-SPI word rectangle. A forced failure at the next requested edge
leaves the 896 × 416 image usable. The main exchange still stages two
frames for this workflow and disposes all three publications on close.
The existing zoom workflow also verifies search, text selection and
geometry through a backing scale change with refinement enabled.

An independent pixel audit compares every interior pixel of five captures
per profile with direct Poppler output and the expected viewport sampling.
The 160-pixel proof stripe contains one color while magnifying the base
and alternates two colors at single-pixel spacing after refinement.
Fit restores the original painted pixels. The cache CTest additionally
verifies letterbox placement, reuse on pan, and retired page/frame keys.

Local archive `nexaui-pdf-detail438.tar.gz` binds the exact source, SDK
receipt, fixture PDFs, direct renderer output, commands, logs and captures.
Readback verified SHA-256
`016d9627fed2f85bf24000d7b5430f831c6d8c8fe7cbdf2aa7e49b328ea51331`.
The local receipt is `pdf-detail438-final.json`.

Larger tiled rasters, GPU interop and physical-output and human assistive
acceptance remain open.

## PDF viewport export checkpoint, 2026-11-18

Consumer `b9dbb8fe5786ac728e1b38e4e5e76de9663f7cfd` uses installed SDK
`85f5d1cab97ff4488062b80d0149dc4efddcb803`. An explicit export action
copies the page number, document generation, camera geometry and output
size to the existing worker. It rereads the pinned PDF and composites
the fitted page under the captured viewport clip with `nx_headless_render`.
The viewport aspect is preserved on a white canvas.

The worker encodes an owned PNG and publishes it at the host-selected
path only after encoding completes. Existing destinations return a
conflict. The UI reports progress and the outcome. File publication
and rendering use the captured view throughout, including when the live
view changes or the Pages dialog closes.

All 62 installed CTests and 49 private Wayland matrix runs passed with
Clang 22.1.3 on Alpine. An additional native static Release export ran
without the optional link reader. The offscreen CTest clears `DISPLAY`
and `WAYLAND_DISPLAY` and verifies exact RGBA pixels, invalid dimensions
and preservation of an existing PNG.

The native workflow captures page 2 at 150% after a 48-logical-pixel pan.
While its render is held, the peer confirms that the target path is absent,
closes the dialog and resets the live view to Fit. The worker publishes
the original panned crop at 802 × 452. The peer reads the PNG, observes
the accessible success message and verifies that a second request for
the same path retains the file. No temporary output remains after the
tested publication and conflict paths.

The four native PNG files are byte-identical. Independent PNG decoding
and comparison with direct Poppler pixels confirm all 362,504 pixels,
including the viewport margins. Local archive
`nexaui-pdf-export440.tar.gz` binds exact source, SDK receipt, input PDFs,
PNG output, reference pixels, commands and logs. Readback verified SHA-256
`d7ad60ff9bab88242139e4fa43f98b84c2f0235251c88bec58b380866ac5cbb4`.
The local receipt is `pdf-export440-final.json`.

Product destination selection and print integration remain open. The
fixture uses one host-supplied destination and a fixed output size. The
offscreen interface takes caller-supplied dimensions and scale.

## PNG image viewer checkpoint, 2026-11-18

Consumer `8dff6bbef28bd02f85a80f3acd587b130767ae91` uses installed SDK
`85f5d1cab97ff4488062b80d0149dc4efddcb803`. The PNG producer fits a single
libpng decode into the existing viewport frame and hands its original
pixels to the UI. The decoded raster is owned independently of the
staged viewport. It can replace the draw bitmap during zoom only while
generation, resize and sequence agree. The software presenter finishes
borrowing both images before either owner retires its pixels.

The common camera applies the fitted image bounds on each axis. It
keeps a fitting axis centered, clamps panning on a scrollable axis and
matches the image clip to the visible AT-SPI image rectangle. Its name
and visible label include the admitted source dimensions. Failed decode
leaves the last raster and dimensions visible.

The native case starts with a 112 × 52 landscape, rejects a truncated PNG,
then accepts a 208 × 416 portrait. The horizontal arrow leaves the zoomed
portrait centered. The vertical arrow pans by 48 logical pixels. Changing
backing from 448 × 208 to 896 × 416 preserves this pan. Ctrl+0 restores
Fit image. The native peer observes an AT-SPI Image role with the source
dimensions and window-relative portrait bounds at both zoom levels.

Independent decoding and pixel comparison verify every interior pixel of
five captures per profile, including the transparent margins and partial
alpha over the viewport background. The zoomed image retains both stripe
colors from the source. All six existing PNG viewport captures per main
profile remain byte-identical to the preceding PNG cases.

The installed SDK matrix passed 62 CTests and 52 private Wayland runs with
Clang 22.1.3 on Alpine. A strict PNG-only static Release build adds 12
CTests and one native portrait workflow without the PDF executables.
Local archive `nexaui-png-viewer442.tar.gz` binds exact source, SDK receipt,
PNG inputs, commands, logs and captures. Readback verified SHA-256
`bf48d1f6d777f71602f1fbcfc5e98221b7243ff35e71cf05412c4b3913a812a9`.
The local receipt is `png-viewer442-final.json`.

Image collections and thumbnails, additional photo formats and metadata,
and physical and human assistive acceptance remain open.

## PNG gallery checkpoint, 2026-11-18

Consumer `f7aa9260a63b05a4c4050d0f1ab26f59c2b39488` uses installed SDK
`85f5d1cab97ff4488062b80d0149dc4efddcb803`. An explicit manifest admits an
ordered collection of up to 256 regular PNG candidates. Each accepted
entry has a bounded UTF-8 leaf filename and a pinned descriptor. The
worker checks file size and timestamps before and after reading. The
manifest is released and the descriptors remain owned through worker join.

The image gallery and PDF pages share the existing three-item thumbnail
cache. Each cache key binds collection generation, raster revision, size
and item. Reopening a group reuses its admissions. Changing density or
moving to a different group retires the old thumbnails. The UI draws
card chrome from the current theme. The worker decodes the selected PNG,
and the viewer adopts the new surface only on successful completion.

The native workflow opens a seven-file collection. It verifies selected
state and focus return, then reopens the same page and selects a corrupt
image. The last accepted raster stays visible. Changing to 2× backing
recovers from the old failure and refreshes the thumbnails. The peer
advances through both remaining groups and opens `Küste.png` with Enter.
Each profile records three thumbnail attempts at 104 × 80 and seven at
208 × 160, including the expected corrupt placeholder at each density.
Three main frames are published and disposed on close.

Independent readback verifies the complete first, retained and final
viewports. The two available thumbnail regions match the expected PNG
pixels at both densities. The earlier PDF thumbnail and page captures
remain byte-identical. The gallery CTest checks order, Unicode names,
changed pinned files, duplicate names and rejected path escapes and
symlinks.

The main matrix passes 65 CTests and 55 private Wayland runs on Alpine
with Clang 22.1.3. A PNG-only build adds 13 CTests and one native run.
Local archive `nexaui-png-gallery444.tar.gz` binds exact source, SDK
receipt, manifest, images, logs and captures. Readback verified SHA-256
`97ae02e0b936148eac160a521e25be8bfc0fcffdc366e487e7efc79748c58dbe`.
The local receipt is `png-gallery444-final.json`.

Product collection selection, live filesystem refresh, more photo codecs,
and human assistive acceptance remain open.

## JPEG gallery checkpoint, 2026-11-18

Consumer `00ca458e3ff9047a80bed2ce0815a978dfefb88d` uses installed SDK
`85f5d1cab97ff4488062b80d0149dc4efddcb803`. Optional JPEG support in
Example 38 reuses the pinned gallery descriptors, existing worker, three
preview slots and zoom/pan camera. The private content snapshot carries
the oriented original size alongside the fitted raster. The visible
dimensions and AT-SPI Image name are read from the accepted snapshot.

The checked packages are TurboJPEG 3.1.3-r0 and libexif 0.6.26-r0 from
Alpine 3.24. The harness verifies the APK signatures and extracts the
libraries and headers to a private prefix. Link inspection found the
two libraries only in the JPEG-enabled profiles. TurboJPEG sets a 64 Mi source-pixel
limit, a 64 MB intermediate-buffer limit and a 128-scan limit, and
treats warnings as decode failures. The existing gallery input limit is
4 MiB. Each RGBA image buffer stays within 4 MiB and 2048 pixels on
either edge. Rotation can temporarily hold two such buffers.
[TurboJPEG API](https://github.com/libjpeg-turbo/libjpeg-turbo/blob/3.1.3/src/turbojpeg.h)
[libexif API](https://github.com/libexif/libexif/blob/v0.6.26/libexif/exif-data.h)

The fixture verifies all eight EXIF orientations with both byte orders,
an invalid orientation tag, a missing end marker and a progressive
4096 × 2048 image. Its view shows a 512 × 1024 raster and reports
2048 × 4096 as the oriented original. The independent native peer
checks preview pixels, normal and zoomed pixels, clipped AT-SPI bounds,
keyboard activation, the previous image after a failed JPEG decode and
the switch back to PNG. The close check accounts for every publication.

The exact-source matrix passes 81 CTests and 59 native Wayland runs on
Alpine 3.24 with Clang 22.1.3 and Weston 14.0.2, including release,
shared SDK, ASan/UBSan, default and PNG-only profiles. Independent
readback checks eight interior samples per quadrant at preview, fit
and zoom. The earlier PNG gallery and PDF thumbnail pixel captures
match the prior run byte for byte. Local archive
`nexaui-jpeg-gallery445.tar.gz` binds the source, SDK and signed package
receipts, fixture inputs, logs and screen captures. Readback verified
SHA-256 `cf5e99b627567561896097f4847664659dab1e912f15673b89a4e56f97094730`.
The local receipt is `jpeg-gallery445-final.json`.

CMYK and lossless JPEG, ICC color management, higher-resolution tiles
for deep zoom, product collection selection and human assistive
acceptance remain open. The example defines the CPU image path. Browser
and guest engine selection and GPU interop remain separate packet 7
requirements.

## Native browser CPU frame checkpoint, 2026-11-18

Consumer `d3f7285144704af550aac94604183d5455aa1bce` uses installed SDK
`85f5d1cab97ff4488062b80d0149dc4efddcb803`. Example 38 can run an offline,
app-owned Chromium Headless Shell 156.0.8078.79 process as an unprivileged
user with a temporary profile and inherited DevTools descriptors. The
engine renders a trusted fixture page at the scenario's CSS extent and
1× or 2× backing density. The native UI exchanges frames through its existing
worker, wake FD, snapshot exchange and software view.
[Chromium headless documentation](https://chromium.googlesource.com/chromium/src/+/main/headless/README.md)
[DevTools screenshot API](https://chromedevtools.github.io/devtools-protocol/tot/Page/#method-captureScreenshot)

Per request, the worker sends generation, resize and physical extent over
the private Unix socket. A five-second monotonic deadline covers request
transmission and receipt. Encoded and decoded buffers each have a 4 MiB
limit, and each edge is limited to 2048 pixels. A successful PNG must
decode to the exact requested extent before the worker can publish RGBA8.
Published Chromium snapshots use full-frame copies. The app keeps ownership
of tab identity and the browser process.

The independent native peer replaces a tab while its already rendered
frame is held, then releases it and checks that the retired generation
never appears on screen. It observes the replacement's pixels and
AT-SPI viewport, kills the fixture browser through the host's test
control, and verifies the retained image and accessible failure state.
The app starts a fresh engine and the viewport displays its pixels.
The same 448 × 208 CSS viewport renders into 896 × 416 physical pixels
at 2× backing. The peer verifies keyboard traversal into and out of the
viewport, host close, four snapshot disposals and browser exit status.

An independent socket probe covers a valid frame, an empty response,
an invalid requested extent, a valid PNG with the wrong dimensions,
a truncated PNG, an over-budget response and a stalled partial reply.
It verifies that errors preserve the caller's outputs and that the
stalled transfer cannot extend the deadline. Both the normal and
ASan/UBSan probes pass.

The exact-source matrix passes 81 CTests, 62 native Wayland runs and
14 transfer probes on Alpine 3.24, Clang 22.1.3 and Weston 14.0.2.
The browser case passes in the release, shared SDK and ASan/UBSan
profiles. Default and PNG-only controls verify the build option in
its disabled state. Independent pixel readback covers pending,
rejected, current, retained, recovered and scaled viewports. The
earlier JPEG and PDF captures remain byte-identical.

Local archive `nexaui-browser-frame446.tar.gz` binds exact source,
SDK and signed package receipts, results, captures and the independent
probe. Readback verified SHA-256
`b9540d809b6a8df363839395a703086e895e0f9eb5ad54243bbec4733da56259`.
The local receipt is `browser-frame446-final.json`.

The product engine decision, web navigation and permissions, body input and
IME candidate geometry, engine accessibility subtree mapping, incremental
damage and GPU interop remain open. This fixture exercises host keyboard
and AT-SPI controls around browser-rendered pixels.

## Native browser text and IME checkpoint, 2026-11-18

Consumer `b98aa6d0e7bc3d402089136f42d0d3013e11851a` uses installed SDK
`85f5d1cab97ff4488062b80d0149dc4efddcb803` and Chromium Headless Shell
156.0.8078.79. Example 38 opts into native Wayland IME transaction
boundaries. The host admits input after Chromium confirms focus on its
editable element and publishes a matching frame and caret. Chromium
owns the editor state. The app advertises the editor identity and
revision with empty surrounding text. The host batches complete native
commits and checks their editor identity, revision and flags.

The worker accepts up to eight queued edit operations and 256 UTF-8
bytes per operation. It forwards composition separately from commit.
The Chromium adapter converts preedit offsets to UTF-16 for DevTools
and returns the CSS caret with the updated frame. The host validates
that reply against the accepted frame sequence and viewport, adds
the viewport offset and advertises the logical rectangle to Wayland.
[DevTools IME commands](https://chromedevtools.github.io/devtools-protocol/tot/Input/#method-imeSetComposition)

An independent native peer enters the page with Return, types `a`
and uses Weston Compose to produce `ü`. Chromium reports a live
preedit while the committed value remains `a`, then reports the exact
committed value `aü`. The Wayland trace records both commits and
the changing candidate rectangles for the same 448 × 208 CSS view
at 2× backing. The peer verifies the host shortcut after a commit,
re-enters the same editor, and activates **Release page** through
AT-SPI during a second composition. Chromium blurs and cancels the
draft. Focus returns to the host Close control and a later native key
leaves the engine's committed value intact.

Independent guard probes exercise a complete multi-scalar commit, a
stale revision, unsupported deletion, rejected or filtered input, an
oversized commit and a partial commit interrupted by host release.
The earlier frame transfer checks still cover the deadline, size and
format bounds.

The exact-source matrix passes 81 CTests, 65 native Wayland runs,
10 input guard probes and 14 frame transfer probes on Alpine 3.24,
Clang 22.1.3 and Weston 14.0.2. The browser editor case passes in
release, shared SDK and ASan/UBSan profiles. Readback confirms six
candidate rectangles at the Wayland protocol boundary, two
deactivations and both expected commits in every profile. The peer
captures the preedit and committed glyphs, and the independent
comparison locates their pixel differences inside the browser editor.
Each run closes with 12 publications and 12 disposals.

Local archive `nexaui-browser-ime447.tar.gz` binds the source, installed
SDK and package receipts, native logs, wire traces, captures and guard
probes. Readback verified SHA-256
`b7c03f9573345dcc54acf6211c8fb2857cf72ee611ad6f4d1a5d8605005afd50`.
The local receipt is `browser-ime447-final.json`.

The fixture retains one edit target and tests
Wayland text-input v1 through Weston simple-IME. Page pointer and
wheel routing, non-text key editing commands, surrounding deletion,
text-input v3 acceptance for this integration, the engine accessibility
subtree, arbitrary navigation and physical IME review remain open.

## Native browser mouse and wheel checkpoint, 2026-11-18

Consumer `38ff790b5853631a4ffb66f90b7abb26d4070367` uses installed SDK
`85f5d1cab97ff4488062b80d0149dc4efddcb803` and Chromium Headless Shell
156.0.8078.79. The existing input adapter and browser fixture share a
half-open viewport hit test. Active browser input converts logical
coordinates relative to the viewport into CSS pixels with 1/256-pixel
resolution. The fixture maps each abstract wheel unit to 40 CSS pixels,
with positive Chromium deltas toward the end of the scroll area.
Browser operations use the existing bounded worker queue and publish
sequence-matched CPU frames. Consecutive pending motion can coalesce.
[DevTools mouse dispatch](https://chromedevtools.github.io/devtools-protocol/tot/Input/#method-dispatchMouseEvent)

The native peer clicks a fixture button and reads back its color change.
Chromium reports the paired release at CSS coordinates `365,48` while
the UI receives an 896 × 416 backing frame for the 448 × 208 view. A
physical wheel step scrolls the page's inner panel by 40 CSS pixels,
and the peer reads the newly exposed color from the native framebuffer.
Moving out of bounds cancels browser pointer input. Wheel activity
outside the viewport and after host release leaves the scroll position
intact.

An AT-SPI **Release page** action during a held mouse button cancels the
engine drag and releases the button off the page. Chromium reports
zero held buttons and the original click count. The native release
drains without activating a host control. A new click after re-entry
activates the page button exactly once. Host-originated gestures keep
their ownership across the viewport boundary. The worker can complete
release on the prior geometry when resizing, and the surface retires
the accompanying old pixels by request identity.

The exact-source matrix passes 81 CTests, 68 native Wayland runs
and 34 independent probes on Alpine 3.24 with Clang 22.1.3 and
Weston 14.0.2. Each release, shared SDK and ASan/UBSan pointer run
reports two clicks, one wheel event, one forced release and 19
snapshot disposals for 19 publications. The independent readback
confirms both changed colors in the native screenshot. The prior
browser CPU captures are byte-identical and the text input cases
still report the same Wayland commits.

Local archive `nexaui-browser-pointer448.tar.gz` binds exact source,
SDK and package receipts, test results, engine receipts, captures
and probe sources. Readback verified SHA-256
`16e23ed63a6804aeff3bd50c5cd614ef444f1b969b9dadac8f5aaa668cb09e43`.
The local receipt is `browser-pointer448-final.json`.

The browser case covers local mouse buttons and a vertical wheel step
on the offline test page. The independent routing probe also checks
horizontal and fractional deltas, boundary clipping, motion
coalescing and leave before focus dispatch. Native resize during an
active drag, touch, relative input, OS capture, production
drag-and-drop, general web navigation and the Chromium accessibility
subtree remain open.

## Native Chromium accessibility subtree checkpoint, 2026-11-18

Consumer `4bffe5f40026e2402d454fa60b852f0d099537a3` uses installed SDK
`85f5d1cab97ff4488062b80d0149dc4efddcb803` and Chromium Headless Shell
156.0.8078.79. The offline fixture uses Chromium's accessibility tree
for names, roles and parentage, and Chromium's DOM quads for visible
geometry. It projects groups, read-only heading and paragraph text,
images and buttons into the existing NexaUI semantic tree. An owned
frame carries up to 32 parent-first nodes with names of
up to 256 UTF-8 bytes. The surface validates these with the same
generation, resize and sequence as the raster.
[Chromium Accessibility protocol](https://chromedevtools.github.io/devtools-protocol/tot/Accessibility/)
[Chromium DOM geometry](https://chromedevtools.github.io/devtools-protocol/tot/DOM/#method-getContentQuads)

The independent AT-SPI peer checks the viewport, document and content
group ancestry, reads `Grün, café and 👋.` as read-only text, and checks
the image's accessible name and clipped 12 × 24 logical extent at both
1× and 2× backing. It rejects the retired tab's late subtree. The current
document stays readable during the forced browser process failure, and
its native objects become defunct after replacement. During the backing
change, objects from the retired viewport are withdrawn before the
accepted frame repopulates it.

At 2× backing the peer focuses **Show notes** and presses it via native
AT-SPI. The worker targets the button using the accepted document epoch
and browser node ID. Chromium applies the action once and returns the
updated pixels and accessible text. The peer reads `Notes opened ✓.`
from the same live AT-SPI text object and checks the changed button color
in the native framebuffer.

The surface CTest rejects duplicate node IDs, invalid parentage, invalid
bounds, malformed UTF-8 and unknown roles without overwriting the
last accepted subtree. Transport probes cover bounds, packet framing,
stale document epochs and successful RGBA8 plus metadata transfer.

The exact-source matrix records 86 passing CTests, 71 passing native
Wayland scenarios and 12 passing transport probes on Alpine 3.24 with
Clang 22.1.3 and Weston 14.0.2. The new AT-SPI scenario passes on its
first run in release, shared SDK and ASan/UBSan. Independent pixel
readback confirms the unchanged document during failure and the new
button color after activation. Each run accounts for five frame
publications and five disposals.

Seventy native scenarios passed on their first run. The existing
ASan PDF link case failed at the link lookup or bounds assertion.
Three retries stopped during Weston startup or AT-SPI registration.
The fourth retry passed against the same source and build with
byte-identical Weston files and font copied into the Linux filesystem.
The evidence retains all attempts.

Local archive `nexaui-browser-ax449.tar.gz` binds exact source, SDK and
package receipts, tests, retries, captures and probe sources. Readback
verified SHA-256
`d1e53feda25dfd2818677d7ebd88f7434bda596d440904f68708f79b5cd555d5`.
The local receipt is `browser-ax449-final.json`.

Full HTML role mapping, editable and linked document actions, complex
browser trees including iframes, and human assistive acceptance remain
open. This case exercises one offline document and one browser button.

## Native QEMU guest console checkpoint, 2026-11-18

Consumer `96926666338e25174f46dfe31e320d7ef8212cb8` uses installed SDK
`85f5d1cab97ff4488062b80d0149dc4efddcb803` and QEMU 11.0.3. The
app-owned, unprivileged TCG process boots a small real-mode VGA
fixture with 32 MiB RAM and a temporary read-only boot image. The
host has one private Unix QMP socket, and the guest has no NIC.
The host controls declare clipboard, file, network, microphone and
device sharing unavailable. QEMU, its firmware and dependencies
come from signed Alpine 3.24 packages extracted to a private prefix.
[QEMU invocation](https://www.qemu.org/docs/master/system/invocation.html)
[QEMU QMP display and input commands](https://www.qemu.org/docs/master/interop/qemu-qmp-ref.html)

The worker admits complete 640 × 400 VGA PNGs from QMP screendump and
uses the existing fitted-image producer to preserve the whole guest
display at 1× and 2× backing. The native image exposes its source
dimensions and fitted bounds. A held frame from the previous view
fails the existing generation check. **Pause guest** changes the QMP
run state to paused and retains the last image. The pause receipt has
an explicit wire marker, and **Resume guest** waits for a running
QMP state before returning a new screenshot.

The host reuses the normalized input gate and queues admitted keyboard
batches to the worker. The independent native peer enters the guest,
holds A, and reads the VGA color generated by the guest's keyboard
handler. **Ctrl+Shift+F12** releases the guest keys and moves focus
back to the host. A later key outside the guest does not reach it.
After re-entry, native focus loss also releases a held key. The guest
records both make and break edges. **Disconnect** withdraws console
input and retains the last image while QMP still reports a running
guest. The app sends QMP quit on fixture close and joins the process.

The exact-source Alpine 3.24 matrix passed 100 CTests, 75 native
Weston scenarios and 14 additional transport probes. The native QEMU
case passed in Release, Shared, ASan/UBSan with leak detection, and
the guest-only build with browser and PDF options disabled. The
default and PNG-only builds also passed. The audit verified five
compositor captures per guest run, matching publication and disposal
counts, and a successful QEMU exit.

On the first matrix setup, the PNG-only and guest-only CMake
configurations reached the PNG dependency check and timed out. The
restarted matrix uses the same Git source archive and stages the
matching PNG headers and live logs on the Linux filesystem. The
local evidence archive is `nexaui-qemu-guest450.tar.gz` (SHA-256
`2a70bdf32ad618e401ee127d6d6423e7d76f6ded6a70fb033bdbe5b94d6c2029`).
Its entry hashes and matrix totals are recorded in
`qemu-guest450-final.json`.

The standalone fixture covers one VGA guest and normalized keyboard
input. A complete graphical guest OS, guest mouse capture, persistent
storage and reset preview, resource accounting in the UI, sharing
grants, guest accessibility and a production VM engine decision remain
open.

## Native QEMU guest pointer checkpoint, 2026-11-18

Consumer `ce45c698ed6821773dac834d93661d2e7b93b3c4` includes relative
pointer motion and the three primary mouse buttons in the same private
QEMU 11.0.3 console and installed SDK
`85f5d1cab97ff4488062b80d0149dc4efddcb803`. The host uses the full
fitted image at 2× backing to translate logical pointer events to
source coordinates. Each entry establishes a fresh motion reference.
Adjacent pending motions can coalesce between button and key edges.

The bounded app-owned transport validates each batch before dispatch.
The fixture sends each button transition in order and splits signed
relative motion into impulses the PS/2 device can report. The guest
reads the auxiliary port, draws a white pointer or a yellow held
pointer, and traces its received edges. The independent Wayland peer
sees movement and dragging in compositor captures. It holds both a
key and a button across the host shortcut and across native focus loss.
Moving into the letterbox revokes input and releases the held button.
The explicit Disconnect action waits for pending input before showing
the disconnected state. QMP still reports the guest running. The
fixture then sends QMP quit and observes a successful process exit.
[QEMU 11.0 input schema](https://github.com/qemu/qemu/blob/v11.0.0/qapi/ui.json#L1010)
[QEMU 11.0 PS/2 input handler](https://github.com/qemu/qemu/blob/v11.0.0/hw/input/ps2.c#L759)

The initial matrix stalled in CMake while its processes were waiting
on the Windows filesystem. With a Linux working directory the six
profiles built and passed CTest before the full Windows volume caused
WSL ext4 I/O failures and a read-only filesystem. A private RAM-backed
overlay for temporary files let the next matrix run without changing
the shared WSL mount. Three native runs at
`cb861f0c1a7c504d3a979576ffe07aaf4849528c` hit the fixture's 150-render
ceiling. The extra mouse stages now use the existing 220-render ceiling
for native pointer workflows.

The corrected revision passed 100 CTests across six build profiles,
75 native Weston scenarios and 28 additional transport, receiver and
input-gate probes. Release, Shared, ASan/UBSan with leak detection and
guest-only each verified ten compositor captures, eight key edges,
twelve mouse-button edges and three motion impulses. Each run matched
frame publications to disposals and observed a successful QEMU exit.
The local evidence archive is `nexaui-qemu-guest-pointer451.tar.gz`
(SHA-256 `51315628ba931243140877cc7630256d7cd76ea40856d80756a7b2daf87ef968`).
Its member hashes are recorded in `qemu-guest-pointer451-final.json`.

This fixture supports the three primary buttons with a relative PS/2
device. Wheel and side buttons, a graphical guest OS, guest accessibility,
storage and reset review, resource accounting, separate sharing grants
and a production VM engine decision remain open.

## Native Alpine desktop guest checkpoint, 2026-11-18

Consumer `821f3bcb0d84557cda4288c74290aac0d1224574` uses installed SDK
`85f5d1cab97ff4488062b80d0149dc4efddcb803`. The optional desktop case
starts QEMU 11.0.3 in TCG with 768 MiB RAM and an 800 × 600 VirtIO VGA
output. It boots Alpine 3.24.4 with Linux `6.18.63-0-virt` and Weston
14.0.2 from a caller-supplied kernel and RAM filesystem. The signed
Alpine minirootfs and 144-package inventory are recorded locally,
along with the signatures and hashes of the three matching QEMU
display module packages.
[Alpine releases](https://alpinelinux.org/downloads/)
[QEMU direct Linux boot](https://www.qemu.org/docs/master/system/linuxboot.html)
[Weston DRM and seat management](https://wayland.pages.freedesktop.org/weston/toc/running-weston.html)

The QMP owner waits for the guest terminal and a visible scanout on
the selected head. The host fits and identifies the entire guest
screen at 1× and 2× backing. The guest panel and terminal decoration
survive pause and resume. Entering the guest enables native keyboard
and relative pointer input. The guest terminal receives the test line
and repaints after a delay. The host's idle frame requests bring the
changed pixels into the NexaUI window without another input action.
The Linux guest's independent evdev reader observes relative motion
and a button press. The host shortcut releases the held button.
Disconnect retains the desktop image. QMP reports the guest running
with the input ledger drained before fixture close and process exit.

The development attempts identified a separately packaged GPU
module, the initramfs capacity of the default tmpfs root, an
unsupported seatd argument and a delay between guest startup and the
first visible DRM scanout. The native test's first color comparison
used a different ANSI green than Weston draws. The final probe matches
the guest's captured RGB value. The first Windows Git archive exported
the guest init as CRLF. The scoped LF attribute keeps its bytes identical
to the executable in the tested initramfs with `core.autocrlf=true`.

The exact-source matrix passed 100 CTests across six profiles and 79
native Weston scenarios. The Alpine desktop and the VGA diagnostic
workflow both passed on Release, Shared, ASan/UBSan with leak detection,
and the guest-only build. Each desktop run includes five compositor
captures and a running guest with no held input before QEMU exits.
The matrix used private RAM-backed temporary files while the shared
WSL filesystem remained read-only. The local evidence archive is
`nexaui-qemu-guest-desktop452.tar.gz` (SHA-256
`8e57481f216fc746e211bf6570fc5172be6015d57e6328d8ae64fda5386025e2`).
`qemu-guest-desktop452-final.json` records the archive members and the
kernel, initramfs and init-script hashes.

The fixture covers a single display, a Wayland desktop shell and its
terminal. Guest accessibility export, IME, wheel and side buttons,
production refresh budgets, persistent storage and reset review,
resource accounting and sharing grants remain open.

## Native QEMU guest storage and resources checkpoint, 2026-11-18

Consumer `cf04cbc77d45e2bb3e336375991c52ac8aa1ea34` uses installed SDK
`85f5d1cab97ff4488062b80d0149dc4efddcb803`, QEMU 11.0.3 and the
Alpine 3.24.4 desktop hardware profile. The optional storage harness
uses the host's mke2fs 1.47.4 to format a freshly created 16 MiB
regular file as ext4. It attaches just that volume to the offline
guest as a VirtIO disk. The kernel and modified initramfs are
recorded separately with their exact digests.

The keyboard-accessible Resources dialog requests its own bounded
reply from the app-owned worker. It uses QMP for run state, CPU count,
assigned RAM, disk capacity and access, and cumulative device reads
and writes. Request completion has its own ticket. Opening releases
guest input, and dismissing restores focus to Resources. A fresh
query after Disconnect still reads the running VM's counters while
the console retains its last desktop image.
[QEMU machine queries](https://github.com/qemu/qemu/blob/v11.0.0/qapi/machine.json#L1191)
[QEMU block information and statistics](https://github.com/qemu/qemu/blob/v11.0.0/qapi/block-core.json#L1186)

The native peer reads the initial and later write totals through
AT-SPI. The guest saves and unmounts a fixture file before the app
joins QEMU. A new QEMU process attaches the same disk read-only. The
guest mounts ext4 without replay and checks the file's exact contents.
The owner verifies positive read bytes, zero write bytes, and the
same SHA-256 before and after the read-only boot.

The first probe queried raw-node statistics and got zero counters
after ext4 had mounted. The corrected owner correlates the named
image node to its VirtIO device and reads that device's counters.
One run at `7a0eb0d` timed out waiting for the first visible guest
scanout after the terminal signalled ready. That log is retained.
The first dialog render also used the host's dark canvas text on a
dark panel. The final dialog uses the theme text color. The capture
audit compares the rasterized resource rows against the panel using
the sRGB contrast calculation.
[W3C contrast calculation and rationale](https://www.w3.org/WAI/WCAG22/Understanding/contrast-minimum.html)

The final revision passed 100 CTests across six profiles, 83 unique
native Weston scenarios, and 28 resource-reply probes. Four graphical
scenarios in the parallel native run timed out before the guest's first
readiness message. All four passed one at a time with the same built
revision and a byte-verified private RAM copy of the QEMU runtime.
The original failures and successful reruns are included. In each
storage profile the AT-SPI write value changed from 1,024 to 22,528
bytes. The read-only boot read 385,024 bytes and wrote zero. The five
resource rows measured 12.85:1 against the panel in all four captures.
The local archive `nexaui-qemu-guest-storage453.tar.gz` has SHA-256
`c551cf8ada0f6bdb9605f58cceb8be59019dff9e15161c1166f0066a375a272d`.
`qemu-guest-storage453-final.json` records its member hashes and
per-profile measurements.

The disk I/O labels represent cumulative device bytes. Live CPU and
memory usage, guest filesystem occupancy, persistent disk ownership
across independent app launches, reset preview and recovery,
per-channel sharing grants and guest accessibility remain open.

## Native QEMU reset preview checkpoint, 2026-11-18

Consumer `38d562ecead3aead49a231002b2c109f96a03326` uses installed SDK
`85f5d1cab97ff4488062b80d0149dc4efddcb803`, QEMU 11.0.3 and the
previously verified Alpine storage boot media. The harness copies
the newly formatted 16 MiB disk into a distinct read-only baseline.
The VM owner assigns an opaque 128-bit environment ID and records
the work disk identity, baseline identity and SHA-256.

The separate preview request checks the live raw VirtIO disk and
revalidates the baseline bytes before publishing the bounded reply.
It presents the exact environment ID, full SHA-256, the affected disk's
size and what a restore would replace. The fixed offline fixture has
zero external write grants. The worker reply carries no sharing
authority. Invalid or unavailable scope produces a visible unavailable
state. The previews do not issue engine mutations.

The independent AT-SPI peer opens the first preview by keyboard,
records the scope and dismisses it with Escape. After the guest saves
data and the console disconnects, the peer reads the same scope and
dismisses it with the keyboard Close action. Both routes restore focus
to the review button. The work disk has diverged from the original
baseline, and an independent read-only boot verifies the retained
guest file with zero write bytes and unchanged disk hash.

The first exact smoke could not read Python's UUID module from the
damaged WSL filesystem. The owner now uses an opaque ID directly from
the OS random source. A later standalone smoke omitted an AT-SPI
runner from its reduced source archive. Final builds extract the
full Git archive and use a byte-verified QEMU runtime in private RAM.

The final revision passed 100 CTests across six profiles, 83 unique
native Weston scenarios and 28 focused checks. The preview was
verified in Release, Shared, ASan/UBSan with leak detection and the
guest-only build. One ASan read-only reboot timed out after the
preview and write stages had passed. The complete isolated rerun
passed on the same build, and both logs are retained. All eight
preview rows measured 12.85:1 against the panel in each profile.
The local archive `nexaui-qemu-reset-preview454.tar.gz` has SHA-256
`e2ee3f39e0563b4cbea2cee82715adef63303af095000b699e81abb581c41963`.
`qemu-reset-preview454-final.json` records the profiles, exact scope
values, archive members and boot artifacts.

Product-owned disk enrollment across app launches, applying a reset,
recovery after uncertain operations, and reviewing concrete external
write grants remain open.

## Native QEMU confirmed checkpoint reset, 2026-11-18

Consumer and installed SDK `841d5620accdcc5c5ce5222f614e5333b5afdb08`
were built from the same Git archive, SHA-256
`4705f12a356910af8fc2213a7b52080dcb86ad687ec6935802df97ee56fb2ca5`.
The test used QEMU 11.0.3, Alpine 3.24.4, Weston 14.0.2 and Clang
22.1.3. The guest initramfs contains the matching executable `/init`
from that revision. The boot artifacts and package manifest are tracked
by the local receipt.

The `guest-reset` test creates a fresh 16 MiB ext4 work disk and baseline.
After console disconnect, the host dialog gives the user the environment
ID, checkpoint SHA-256, affected disk and consequences. Keyboard
confirmation sends the exact environment and checkpoint to the owner.
The owner checks the active disk and immutable baseline, stops and joins
QEMU, retains the old disk, and atomically installs the checked copy.
The test drops the success reply. A bounded status request observes the
applied result, and reopening the dialog displays it again. Each run
records one disk replacement and one status request.

The independent AT-SPI peer verified keyboard confirmation, result
reading, and focus restoration on dismissal. The rendered preview and
outcome text reached at least 12.85:1 in both captures. `debugfs` read
the exact 28-byte guest-written file from the retained disk. A new QEMU
boot mounted the installed image read-only, observed the original
baseline, read 384,000 device bytes and wrote zero. The restored disk
matched the baseline SHA-256 before and after that boot.

The exact revision passed 54 CTests across four build profiles, seven
private native test runs and 25 focused owner and wire checks. Reset
passed in Release and with AddressSanitizer, UndefinedBehaviorSanitizer
and leak detection. The default, PNG, VGA, desktop and storage scenarios
also passed. WSL I/O errors blocked the previous runner. Retained setup
logs show the isolated Alpine test root and the retries after resolving
its runtime library and D-Bus environment failures. The previous full
optional-engine matrix was outside the scope of this checkpoint.

The local archive `nexaui-qemu-reset-apply455.tar.gz` has SHA-256
`13236ab447856cc4cb0848b2a6c480441d051e109909ccb6051d156148163bdf`.
`qemu-reset-apply455-final.json` lists the member hashes, boot artifacts,
tested profiles, bound scopes and independent readback measurements.

Product-owned disk enrollment, durable recovery across owner restarts,
individual external write grants and guest accessibility remain open.

## Native QEMU reset owner replacement, 2026-11-18

Consumer and Wayland-only SDK source
`c39fb7b0e269d9fb2a206d57b71618c1714827c4` were built from Git archive
SHA-256 `fa6814b66c39dcb7a41b07f625f94bc2c4853ec73196e2da008e2e5be84ab55b`.
The isolated Alpine environment uses Clang 22.1.3, Weston 14.0.2 and
QEMU 11.0.3 with the boot artifacts from the confirmed-reset checkpoint.

The reset owner joins QEMU, installs the checked disk and flushes the
directory. It records a bounded, versioned receipt for the confirmed
environment and checkpoint with identities for the installed, baseline
and retained images. The owner flushes the receipt and its directory
entry before returning success. The replacement owner opens the receipt
read-only, checks the scoped IDs and reobserves all three image identities
and hashes. Missing or inconsistent evidence yields unknown. A different
scope is rejected. The replacement owner refuses restore requests.

In `--case guest-reset --guest-owner-restart`, the original owner exits
after committing the receipt and before replying. The harness retains
the private listener while starting a new process. Each native run sees
distinct owner PIDs, exactly one disk replacement and one status read.
The independent AT-SPI peer observes the restored result before and after
reopening the dialog and verifies focus restoration. The retained disk
still contains the exact 28-byte guest file. A separate, read-only QEMU
boot checks the installed baseline and its stable hash.

The exact revision passed 54 CTests across four fixture build profiles,
eight native test runs, six receipt regression tests and 15 existing
owner probes. Owner replacement passed in Release and under ASan/UBSan
with leak detection. The original dropped-response, VGA, desktop and
storage workflows passed alongside the default and PNG controls. The
receipt tests cover malformed or missing receipts, byte and inode
changes, duplicate publication, failed flush and rejection of replay by
another process.

The local bundle `nexaui-qemu-reset-recovery456.tar.gz` has SHA-256
`32ae11e21fcdbb36247fd7e22e97c6f4075e83c62c257dffff974d49f9f972a5`.
`qemu-reset-recovery456-final.json` records profile results, library and
artifact hashes, PIDs, exact scopes and archive member hashes.

Recovery here assumes that the mounted private work directory and the
harness remain available. Product disk enrollment across UI launches,
recovery across host or storage failure, external sharing grants and
guest accessibility remain open. The optional Browser/PDF/JPEG matrix
has separate prior evidence.

## Native QEMU file sharing and reset scope, 2026-11-18

Consumer and Wayland-only SDK source
`ae96d5d92c5b61ed1de88e04d0cdab9db6bc9d74` were built from Git archive
SHA-256 `25da44e4b054081c450215c6cab71240d91af95c53ecf634ea989353f369a946`.
The guest boot uses the same Alpine kernel and an updated initramfs with
the matching `/init` and the `9p` and `9pnet_virtio` modules.

`--case guest-reset --guest-file-share` grants the private test guest one
writable, harness-created directory at launch. QEMU 11.0.3 exports it as
a VirtIO 9p device. The owner checks the live device tag and directory
identity when reading permissions and again before restoring the disk.
The owner generates the environment ID for the immutable launch scope.
The desktop Sharing dialog shows each channel and the exact granted host
directory. The reset preview identifies that directory and explains what
happens to its files. [QEMU file exports](https://www.qemu.org/docs/master/system/invocation.html)
[Linux VirtIO 9p mount](https://docs.kernel.org/filesystems/9p.html)

The independent AT-SPI peer opened Sharing by keyboard, read the five
channel rows and the exact path, and checked focus restoration. It read
the same grant after console disconnect and in the reset review. The
guest used the 9p mount to write its host file. The peer compared the
bytes and file identity before and after restore. A replacement owner
answered the pending status read, and a separate QEMU boot confirmed
the restored work disk with read-only access. The harness verified the
external file again after that boot.

Release and ASan/UBSan with leak detection both passed the file-sharing
workflow. The measured contrast of the six sharing rows and the path
in the reset preview was 12.85:1 in both captures. The same revision
passed 54 CTests across four profiles, ten native test runs and 43
focused checks. The focused checks include malformed sharing packets,
legacy reset preview compatibility, a wrong live device tag, and
replacement or permission changes to the export directory. Native
runs with the default no-grant profile verify the individual
unavailable states and the original reset, desktop and storage behavior.

The initial retained synthetic owner probe needed the added file-share
field initialized. An unshared graphical guest also timed out before
readiness once. The isolated retry passed with the same binaries and
boot artifacts. Both failed logs are included. The local bundle
`nexaui-qemu-file-share457.tar.gz` has SHA-256
`11b8849365dbcf7f8a277b2df9ee51a16f2f2e6669ef0db147f8b2660730ce0c`.
`qemu-file-share457-final.json` records all member hashes, exact
scope, artifact hashes, independent readback and contrast results.

Aetheri's product authority, runtime grants and revocation, the other
sharing channels and the guest's accessibility tree remain open. The
private launch grant and reset evidence are available for consumer
integration with the existing owner model.

## Native QEMU file grant revocation, 2026-11-18

The consumer and Wayland-only SDK were compiled from source
`266404bf1743248f16eb289c0a1c0d99701cc395`, archive SHA-256
`ce8dc4467a0567479f6df0e90548d4a85d2b43e5e5e59a576481320ea9e81488`.
The final runtime candidate is `7e88cd01fbc799debe82743841081a0e86a8f7e2`.
Its three changed runtime files were extracted from archive SHA-256
`3617645a76da510e3782f13648b7dd1eb3532cb6e8f878412d37c0e5826b3b24`.
The compiled sources are identical across the two revisions. The test
uses QEMU 11.0.3, the existing Alpine kernel, and an initramfs with the
matching guest `/init` and console probe.

In `--case guest-reset --guest-file-share --guest-file-revoke`, the
host binds a revoke request to the observed VM environment and export.
QMP accepts a single `device_del` request while the guest still holds
its VirtIO 9p mount. Sharing displays the pending state and offers a
status read. The owner waits for the exact `DEVICE_DELETED` event and
checks that QOM no longer lists the export device. A request with an
uncertain reply stays unconfirmed until the owner observes completion.
[QEMU device removal contract](https://www.qemu.org/docs/master/interop/qemu-qmp-ref.html)

The guest's `detach` command releases the mount. After confirmed
removal, a guest attempt to remount the former export fails. The host
checks that the previously written file retains its bytes and inode.
The reset dialog continues to list the exact host directory where
those writes stay. A separate guest boot verifies the restored work
disk while the external file remains available to the host.

Early runs missed desktop readiness. A retained serial trace showed
the shell exiting after `udevadm settle --timeout=5`, followed by a
guest kernel panic. The corrected fixture allows 30 seconds for udev
and 120 seconds for the complete emulated desktop boot. The owner
reports a startup error as soon as the serial trace records an init
failure or panic. These deadlines cover test startup.

Release and ASan/UBSan with leak detection each passed the complete
cooperative revocation workflow. The independent AT-SPI peer verified
keyboard activation, pending refresh, and focus restoration. Both
pending and revoked status rows measured 12.85:1 in the captures. Each
run logged one device removal request, one confirmed removal, and one
reset. The final candidate passed 54 CTests across four build profiles,
nine native test runs and 61 focused checks. The focused checks cover
stale environment IDs, missing and malformed replies, pending and
unknown outcomes, device presence, repeated commands and export identity
changes. The new wire cases also passed with sanitizers.

The local bundle `nexaui-qemu-file-revoke458.tar.gz` has SHA-256
`610bea62343ac5ba8fd9d02fb0181859f1d861731de0970f751659eb42472091`.
`qemu-file-revoke458-final.json` records the compiled and runtime
revisions, boot artifacts, per-run scopes, host file hash, captures and
member hashes. The bundle includes the unsuccessful boot attempts and
the diagnostic serial trace.

The verified path requires a guest that releases its mount. Runtime
grant creation, an enforced cutoff when the guest keeps a mount,
Aetheri product authority, other sharing channels and guest accessibility
remain open.

## Native QEMU runtime file grant, 2026-11-18

The consumer was compiled from `0ccbde8433bcbcd6d0634ae7ddcd1cb9743ad9cd`.
The selected Git archive has SHA-256
`92c806ddb2ac7a3c2d22c468ff60da79636969c810d26eff18711032c953a394`.
The Wayland-only Release and sanitizer SDK libraries were reused from
`266404bf1743248f16eb289c0a1c0d99701cc395`. The SDK sources, build
configuration and data are identical at the consumer revision. The
audit compares both libraries to the previous evidence receipts and
compares the boot image's executable `/init` with the archived source.
The native environment uses Alpine/Weston 14.0.2, QEMU 11.0.3 and
Clang 22.1.3.

With `--case guest-reset --guest-file-grant`, the private owner registers
one prepared directory at VM startup. Sharing shows its exact path
and **Files: access off**. The independent peer uses the keyboard to
activate **Grant file access**. The owner asks QMP to attach one VirtIO
9p device. **Check sharing status** reads the result and exposes
**Files: read/write** when the named device and mount tag agree. An
uncertain reply preserves the pending or unknown result until it can
be observed. The owner issues a single attach request for this grant.
[QEMU 11.0.3 device command contract](https://github.com/qemu/qemu/blob/v11.0.3/qapi/qdev.json)

The guest's first mount attempt fails and the prepared directory starts
empty. The guest mounts the channel after approval and writes a file
through it. The reset review carries a scope binding that changes when
the owner admits the grant. A direct run against QEMU kept the same
environment and checkpoint, observed the changed scope and confirmed that
the owner rejected the earlier review. A fresh review lists the external
directory where previous writes are retained. The keyboard workflow then
revokes consent with guest cooperation, verifies remount denial and
restores the checkpoint. The replacement owner observes one completed
reset. A read-only guest boot verifies the checkpoint, and the host
compares the exported file's bytes and inode before and after reset.

The test set passed 54 CTests across four build profiles, 10 native
runs and 75 focused checks. Release and ASan/UBSan with leak detection
covered the grant, its revocation, and reset-owner replacement. The
host AT-SPI peer checked action availability, status and focus restoration.
The captured **activation pending** and **read/write** rows measured
12.85:1 in both profiles.

The first runs of two launch-grant controls missed the expected guest
terminal color after the write. The controls passed on retry with
the same source, binaries and boot artifacts. The revocation retry also
ran with owner replacement, as in the preceding checkpoint. The
evidence bundle includes the unsuccessful runs.

The local evidence bundle `nexaui-qemu-file-grant459.tar.gz` has SHA-256
`67acb7920c7536b21c1f3e18be6fb8e37589d2ab533dc96566c001a8f90bd401`.
The matching `qemu-file-grant459-final.json` records scope, profile
counts, and hashes for the selected source, boot artifacts,
SDK libraries, native captures and archived members.

The test owner supports one runtime grant for a prepared private
directory. Aetheri's production grant authority, forced revocation
while a guest keeps its mount, the remaining sharing channels and
the guest's accessibility tree remain open.

## Native QEMU file cutoff with a held mount, 2026-11-18

The consumer source is `8e39b38225497c33110dc28c0af99b1ba489f58a` and
its selected Git archive has SHA-256
`15aa7241f9ac9b8538569a6ccb282966fe4b9e2ccb35cae6170c1fd7d87c5759`.
The Release and sanitizer Wayland-only SDK libraries came from
`266404bf1743248f16eb289c0a1c0d99701cc395`. The SDK sources, CMake
profiles and data are identical at the consumer revision. The audit
checks both library hashes and the archived source of the executable
`/init` in the tested guest image. The run uses QEMU 11.0.3, Weston
14.0.2, and bindfs 1.18.4 linked to Alpine libfuse 3.18.3.

The opt-in `--guest-file-bindfs` profile gives QEMU a private FUSE mount
of the prepared directory. Sharing and reset review continue to show
the retained host directory. Bindfs runs in the foreground with direct
I/O and mount-owner-only access. The owner uses the existing explicit
revoke action to terminate the per-export helper. It checks that the
helper has exited and the exact original mount returns `ENOTCONN`
before reporting **Files: revoked**. The disconnected mount stays in
place until QEMU joins, and the owner then unmounts it.
[Linux FUSE connection lifetime](https://docs.kernel.org/filesystems/fuse/fuse.html)
[Bindfs mount options](https://bindfs.org/docs/bindfs.1.html)

The separate QMP run observed the original QEMU process running and
its original VirtIO 9p device still present after cutoff. The guest
used its retained mount and open file descriptor to attempt a write
and also tried to create a file. Both operations failed. The host
verified exactly one original file with unchanged bytes and inode.
The native keyboard and AT-SPI peer verified the pending and confirmed
rows, focus restoration, input re-entry and escape, and no new file.
Reset retained the external write and the prior work disk. A separate
read-only guest boot verified the restored baseline. A replacement
owner observed one completed reset.

Release and ASan/UBSan with leak detection passed the held-mount
workflow. The native **revocation pending** and **revoked** rows
measured 12.85:1 in each build. The audited test set passed 54 CTests across
four profiles, 11 native runs and 81 focused checks. A separate
unprivileged host check observed `ENOTCONN` for a retained descriptor
and a new path. The focused checks also cover invalid source
directories, helper launch failure, stale mount identity, unexpected
helper loss and cleanup.

The first matrix invocation stopped in preparation when the older
stand-alone owner probe used incorrect indentation for its added field.
The corrected invocation passed the native controls and the full matrix.

The local evidence bundle `nexaui-qemu-file-cutoff460.tar.gz` has SHA-256
`d9ddb22aece4fdde93eab991c8f256a5e7fc1242e46aa1b193e641f0c264cb5a`. Its companion `qemu-file-cutoff460-final.json` records
the exact artifacts, SDK libraries, native scopes and archived hashes.

The backend supports one owner-prepared private test directory and
requires Linux FUSE. Revocation covers I/O attempted after the owner
confirms cutoff. Aetheri's production grant authority, the remaining
sharing channels and guest accessibility remain open.

## Native QEMU host-to-guest clipboard, 2026-11-18

The tested consumer source is `1e0a2b64c41efce2477b751980d6344667745ebe`.
Its selected Git archive has SHA-256
`eac47404334edbb1b134332eb7e9a03dfdf8a8da427e87d1e89427708236f22e`.
The Release and sanitizer Wayland-only SDK libraries came from
`266404bf1743248f16eb289c0a1c0d99701cc395`. The SDK sources, build
configuration and data match the consumer revision. The audit verifies
both library hashes and compares the executable `/init` in the boot
image with its archived Git source. The test uses QEMU 11.0.3,
Weston 14.0.2 and Alpine wl-clipboard 2.2.1-r1.

With `--guest-clipboard`, Sharing lists **Clipboard: one-time text to
guest** independently of the file grant. **Review clipboard text**
uses the public nonblocking Wayland read API on the host window.
The owned review accepts up to 192 UTF-8 bytes, four LF-delimited lines
and 32 scalars per line. It rejects invalid UTF-8, NUL and the fixture's
disallowed control characters. Escape and window focus loss discard
an unconfirmed review. **Copy text to guest** submits the exact
displayed snapshot to the private owner.

The owner checks the VM identity, a monotonic ticket and running
guest state. Its private serial channel sends bounded text and a
digest. In the guest, wl-copy offers the text over Wayland and wl-paste
checks the exact bytes before the guest reports receipt. An independent
Wayland client then reads the selection again when the native peer
enters the guest. A transport deadline or mismatched reply retires the stream.
The owner reports uncertain outcomes and refuses duplicate tickets. Normal logs contain state and
receipt evidence without the text itself.
[QEMU character devices](https://qemu.readthedocs.io/en/v11.0.0/system/invocation.html#character-device-options)
[wl-clipboard clipboard source](https://github.com/bugaevc/wl-clipboard/blob/v2.2.1/src/wl-copy.c)

Release and ASan/UBSan with leak detection passed the clipboard-only
workflow. The peer exercises the oversized selection, Escape
cancellation, native focus restoration, exact multiline review,
explicit keyboard send and return to the guest. A combined Release
run covers clipboard plus a separate runtime file grant, forced file
cutoff and reset-owner replacement. The guest observes exactly one
accepted clipboard transfer in each run. The review and confirmation
status rows measured 12.85:1 in each tested profile.

The first native pass received the guest acknowledgement but missed a
specific terminal color. The final fixture focuses the guest terminal
and uses the independent Wayland readback receipt as its acceptance
criterion. The bundle includes the initial log and screenshots. The
audited run passed 54 CTests across four profiles, 12 native
runs and 106 focused checks. The focused checks cover stale, malformed
and missing wire replies, bounded deadlines, source validation and
existing file grant and reset behavior.

The local evidence bundle `nexaui-qemu-clipboard461.tar.gz` has SHA-256
`fdc62e91d92277fe273b60fe08e37dfb9d269301cfb871965b1e2d1e09930f52`. The companion `qemu-clipboard461-final.json` records the
source, boot artifacts, tool hashes, test outcomes and archived hashes.

The proof covers explicit host-to-guest text within the private VM
fixture. Guest-to-host transfers, Aetheri production grant authority,
the other sharing channels and guest accessibility remain open.

## Native QEMU guest-to-host clipboard, 2026-11-18

The exact tested consumer revision is
`36ed386630c2738e271c75621588d4e3856f4172`. Its selected Git archive
has SHA-256 `28cb8a56169e22d34b8bdd89518a0a73b86679229ea351430a98c222754890c6`.
The Wayland-only Release and sanitizer SDK archives came from
`266404bf1743248f16eb289c0a1c0d99701cc395`. Core sources, build
configuration and data match the consumer revision. The audit checks
the archive and SDK hashes and compares the guest image's executable
`/init` with its archived source. The run uses QEMU 11.0.3, Weston
14.0.2 and Alpine wl-clipboard 2.2.1-r1.

**Review guest text** requests a bounded snapshot from the guest's
Wayland clipboard over the private serial channel. The owner checks
the running VM, capability and ticket, then verifies the complete
reply's framing, SHA-256, UTF-8 and the existing review limits. The
host worker delivers one correlated result to the open review. The
host discards an unconfirmed snapshot on Escape or focus loss.
The capability reply keeps the earlier send-only profile readable.

**Copy text to host** offers the reviewed snapshot as the host
Wayland selection after explicit approval. The peer published an
invalid guest selection and observed the original host selection
through a separate Wayland client. It then cancelled a valid review
and observed the original host selection again. After a separate
review and keyboard confirmation, the independent host reader
obtained the exact multiline UTF-8 bytes. A separate QEMU run
confirmed the old accepted snapshot remained intact after the guest
changed its clipboard. It also verified foreign VM rejection,
ticket replay rejection and an unsupported control character.
[Wayland selection contract](https://gitlab.freedesktop.org/wayland/wayland/-/blob/1.26.0/protocol/wayland.xml)
[Guest Wayland selection reader](https://github.com/bugaevc/wl-clipboard/blob/v2.2.1/src/wl-paste.c)

The clipboard-only workflow passed in Release and ASan/UBSan with
leak detection. A combined Release run covered both clipboard
directions alongside a separate file grant, forced file cutoff and
reset-owner replacement. Each of the three runs observed one
outgoing acknowledgement and two valid incoming snapshots. Test
logs record those events without clipboard contents. The review and
offer status rows measured 12.85:1 in each tested profile.

The first full native run passed both clipboard directions and
then reached the previous host trace reader's 16 KiB limit. The
final reader uses the existing 128 KiB full-trace limit and retains
the same pointer assertion. The evidence bundle includes the
initial trace. The reviewed set passed 54 CTests across four
profiles, 12 native runs and 133 focused checks. Additional
checks cover fragment handling, size and UTF-8 limits, malformed,
stale and foreign replies, and the total transfer deadline during
partial progress. The runtime and launch file-grant controls missed a
terminal color on their first runs. Both passed on retries with
identical source, binaries and boot artifacts.

The local evidence bundle `nexaui-qemu-clipboard-import462.tar.gz`
has SHA-256 `48637efd3411301bb3edf09281a02ad44aece40669014e6a63a4b247742aa1a1`. The matching
`qemu-clipboard-import462-final.json` records exact sources, tool and
boot hashes, test outcomes, captures and archived hashes.

The fixture provides explicit bounded text transfers in both
directions. Aetheri's production grant authority, network,
microphone and device sharing, and guest accessibility remain open.

## Native QEMU private network grant, 2026-11-18

The exact tested consumer revision is
`ab73b8b505dde2b60db8ac053b813d935eda1cd4`. Its selected Git archive
has SHA-256 `3ff2fd0f6d02730fc43a0d9a31536faf45bb09e260cac65051e81861c7ddc9b3`.
The Wayland-only Release and sanitizer SDK libraries came from
`266404bf1743248f16eb289c0a1c0d99701cc395`. The compared core sources,
CMake configuration and data match the tested consumer revision. The
audit verifies SDK hashes and the executable `/init` in the guest
boot image. The native runs use QEMU 11.0.3 with libslirp, Weston
14.0.2 and Alpine's VirtIO network driver.

The opt-in `--guest-network` profile starts with the adapter absent.
**Sharing** displays `10.0.2.100:8080` and the network access state.
The original VM owner validates the selected environment before it
attaches a VirtIO adapter. The QEMU user-mode backend enables
`restrict`, disables IPv6 and forwards one TCP endpoint to the
private Unix socket. A fresh read verifies that QEMU realized the
expected adapter, backend and MAC and that the service accepted the
Unix connection. The native peer observes **activation pending**
before checking the resulting active state.
[QEMU 11.0.3 network commands](https://github.com/qemu/qemu/blob/v11.0.3/qapi/net.json)
[QEMU restricted user-mode network](https://qemu.readthedocs.io/en/v11.0.0/system/invocation.html#network-options)

The independent guest terminal checks absence before approval and
receives the exact TCP response after approval. The direct run also
checks a host loopback listener on a dynamically allocated port.
The guest has a default route during this probe, and the independent
host listener receives no connection. Revocation retires the Unix
listener and accepted socket before confirming cutoff. The owner
also requests link-down. The guest attempts another operation on
the held connection and opens a new connection. Neither probe
receives the service response. The guest remains running, with the
original adapter still present in QEMU. The test service records
one request and no post-cutoff request.

The native flow passed in Release and ASan/UBSan with leak detection.
The combined Release run included both clipboard directions, a
separate file grant and cutoff, and reset-owner replacement. The
keyboard and AT-SPI peer verified the endpoint, status, controls and
focus restoration. The network status rows measured 12.85:1 in each tested network profile.
The audited set passed 54 CTests across four profiles, 13
native runs and 166 focused checks. These include a foreign VM scope,
a lost attach reply, adapter identity drift, local socket cleanup
and earlier sharing-wire compatibility.

The first standalone QEMU probe lost a burst of emulated keyboard
events. The paced probe passed against the same guest image. Its
final run uses the archived source revision.

The local evidence bundle `nexaui-qemu-network463.tar.gz` has SHA-256
`fe577c2af805cc4256ca42fdd5fdb257df8141ef5eb5f362a0fd67e9d67444c5`. The paired `qemu-network463-final.json` records the
source, boot and library hashes, native observations and archived
members.

The fixture covers a single private TCP service. General network
policy and Aetheri's production authorization, microphone and device
sharing, and guest accessibility remain open.

## Native QEMU synthetic microphone grant, 2026-11-18

The tested consumer revision is `9dddad76365b076e5f479ceba638738fbe6fe4c5`.
Its selected Git archive has SHA-256
`6f9af0c805f6cd32ed026b9eee04520ce3928e705a198183d9bd17e30c0a740f`.
The installed Wayland-only Release and sanitizer SDK libraries came from
`266404bf1743248f16eb289c0a1c0d99701cc395`. The core source, public headers,
build configuration and data match the consumer revision. The guest uses
QEMU 11.0.3, Alpine Linux `6.18.63-0-virt` and Weston 14.0.2. The guest
image includes ALSA utilities and the generic HDA codec driver.

The optional `--guest-microphone` profile supplies one synthetic tone
through a private ALSA configuration and an inherited nonblocking PCM
descriptor. Playback uses the ALSA null device. The source starts silent.
The host checks the current VM scope, input release and running state before
granting access. Revocation stops new tone samples and drains the owner pipe
before confirming the source state. QEMU may still deliver samples that it
already consumed. The owner retires the feed and joins its worker on close.
The `NXS5` reply adds the microphone state to the existing file, clipboard
and network snapshot. The earlier wire formats still describe their channels.
[QEMU 11.0.3 ALSA audio options](https://raw.githubusercontent.com/qemu/qemu/v11.0.3/qapi/audio.json)
[ALSA PCM input plugin](https://www.alsa-project.org/alsa-doc/alsa-lib/pcm_plugins.html)

The host dialog exposes **Enable test tone** and **Revoke test tone**. The
independent AT-SPI client verifies the state, moves to each action with Tab,
activates with Enter, and checks focus restoration on Escape. The guest opens
one real ALSA capture before consent and keeps it open across the grant and
revocation. Each window spans one second of stereo capture at 48 kHz. The
guest checks both tone polarities in eight 10 ms slices of the active window.
It compares all 96,000 samples in each of the pre-grant, post-revocation and
freshly reopened windows byte for byte with silence. The held post-revocation
window starts after a settling interval. Exact audio latency and worst-case
drain time remain open.

The Release and ASan/UBSan runs passed with the installed SDK. The combined
run passed with both clipboard directions, the private TCP service and the
reset owner replacement. The wider run that also enabled file sharing stopped
at the file workflow before reaching the audio assertions. That combination
remains open. The audited checks cover 54 CTests across four build profiles,
six reset-receipt tests, nine native runs and 22 dedicated wire cases in both
Release and sanitizer builds. The direct VM run checks foreign scope, paused
and held-input rejection, idempotent replies and owner shutdown. The active
and revoked status rows measured 12.85:1 in all three audio runs.

The guest initramfs has SHA-256
`1fc2c27764cb2c5fc8065be448e738f6e468bd6fc4a1a868b5957c9309ce1b96`.
The local evidence archive `nexaui-qemu-microphone464.tar.gz` has SHA-256
`5c8c3db719a86b5295365038ee905d90d7a80cf641bf179dbe5a2c65709935aa`.
`qemu-microphone464-final.json` records the source, SDK and guest image
hashes, measured windows, native results and 187 verified archive members.

Physical microphone selection, production consent policy, hardware device
sharing and the guest accessibility tree remain open.

## Native QEMU read-only USB grant, 2026-11-18

The tested consumer revision is `fadb4ca873011a1d78a825817bbb39df59d75fde`.
Its selected Git archive has SHA-256
`18657a496be2d62882d5e961b0fa147a3a529455344a9bfca9d3811cb57b2545`.
The installed Wayland-only Release and sanitizer libraries came from
`266404bf1743248f16eb289c0a1c0d99701cc395`. The SDK sources, headers, build
configuration and data match the tested consumer. The native fixture uses
QEMU 11.0.3, Weston 14.0.2 and Alpine Linux `6.18.63-0-virt`.

The optional `--guest-usb` profile boots with an empty xHCI controller and
holds a privately generated 1 MiB raw disk. **Attach test disk** checks the
current environment, released guest input and running status. The owner adds
the backing node in read-only mode and attaches the USB storage device to
port 1. **Check device status** verifies the realized storage type, exact bus,
port and serial, plus the read-only image and its path through the block
backend. The host distinguishes the requested and verified states.

**Detach test disk** requests removal. The owner requires the matching
QMP `DEVICE_DELETED` event and absence of the USB device and its block use,
then removes the private backing node and checks it is gone. A mismatch or
failed observation can leave an unknown state. The independent AT-SPI peer
uses Tab and Enter for the actions and confirms focus restoration with
Escape. The `NXS6` wire reply carries the device state alongside the other
channels. The earlier five reply formats remain readable.
[QEMU USB devices](https://www.qemu.org/docs/master/system/devices/usb.html)
[QEMU 11.0.3 removal events](https://raw.githubusercontent.com/qemu/qemu/v11.0.3/qapi/qdev.json)

An independent Alpine client observes absence before consent, reads the
expected bytes from the exact USB disk, verifies its read-only state and
attempts a write that the kernel rejects. After removal, its held descriptor
cannot read the previously unread test sector and a new open obtains no
data. The direct owner probe verifies the backing image digest stayed the
same, foreign environment requests were rejected and QEMU stayed running
until orderly shutdown.

The audited set passed 28 CTests, six reset-receipt tests, four native runs,
25 wire cases in each of Release and ASan/UBSan, and eight owner checks.
The combined native run included the USB disk, synthetic microphone, private
TCP service and both clipboard directions. A separate native run exercised
the network and microphone with USB disabled. The USB status rows measured
12.85:1 across the Release, sanitizer and combined USB runs.

The guest initramfs has SHA-256
`174f850c47d79ecec783a94d85fa383af76ea3c2cfafbc959ec259c499273c20`.
The local evidence archive `nexaui-qemu-device465.tar.gz` has SHA-256
`d45c21f47773336618a622bbe47e4f275c9c36e9953cba2689832d695ebabe29`.
`qemu-device465-final.json` records the source, SDK and boot hashes, exact
test-disk digest, native outcomes and 112 verified archive members.

Physical USB passthrough, device selection and policy on Aetheri, the wider
file-sharing combination and guest desktop accessibility remain open.

## QEMU guest accessibility snapshots, 2026-11-18

The tested source is `d50f6ed82df84d534741614d7a2444585a701e20`. Its selected
Git archive has SHA-256
`0cb0fb9294a98b4baa756539a2893efef2f3d54a90a5a3675bd5bb696199fba2`.
The host used installed SDK `266404bf1743248f16eb289c0a1c0d99701cc395`.
The guest used QEMU 11.0.3, Alpine `6.18.63-0-virt`, Weston 14.0.2,
GTK 3.24.52 and AT-SPI 2.60.5.

The optional reader queries a process selected by the VM owner on the
guest accessibility bus. It requires one showing window and matching
window-local bounds. It produces up to 32 records for the frame, visible
labels and push buttons, with their bounds and disabled state. The GTK
test exposes a changing Unicode label, two buttons and a password field.
The snapshots contain the Unicode change and both buttons. The password
field and its value are absent. An incorrect size, hiding the window while
the process is still live, and exiting the process each produce no snapshot.
[AT-SPI child enumeration](https://gnome.pages.gitlab.gnome.org/at-spi2-core/libatspi/method.Accessible.get_child_at_index.html)
[AT-SPI coordinate systems](https://gnome.pages.gitlab.gnome.org/at-spi2-core/libatspi/method.Component.get_extents.html)

The independent runner captures two matching 800 × 600 QEMU rasters and a
third after the window has retired. The host probe uses the actual rasters
and native guest snapshots to publish frames through the existing surface.
In an 896 × 416 viewport the fitted image starts at x = 171 and spans
554 × 416 pixels. The first button's bounds map to (226, 119, 444, 77).
The published semantic tree exposes read-only Unicode text, button roles
and the disabled state. The update retires the old epoch's identifiers.
The third frame retains the pixels with an empty guest subtree. A truncated
packet and a root with mismatched geometry are rejected.

The host probe passed in Release and ASan/UBSan with leak detection. Forty
CTests passed across the default and two native build profiles. The guest
initramfs has SHA-256
`14b577dcb12e5635c342f2151cdfc514c844eb4485c2fbed0db003f13ae8b8d6`.
The evidence archive `nexaui-qemu-accessibility466.tar.gz` has SHA-256
`0ac6b02a6a10b0d57d84a4ee9e4a57f521d84b3efa7c7eca9312ac75b28fca9d`.
`qemu-accessibility466-final.json` records 26 verified archive members and
the exact guest binary, images and source hashes.

The proven path covers a selected fullscreen window. Continuous transport,
publication through the native host AT-SPI bridge, other window geometries,
guest actions and a screen-reader walkthrough remain open.

## Native QEMU guest accessibility relay, 2026-11-18

The tested source is `f3e290df03a307914a7cdcf3c41c68ec978e7f25` on installed
SDK `266404bf1743248f16eb289c0a1c0d99701cc395`. The optional
`--guest-ax` workflow connects the selected fullscreen GTK window to the
Wayland console through a private QEMU serial channel. The owner compares
native AT-SPI snapshots around the QEMU screenshot. Missing or invalid
metadata leaves a valid raster with an empty guest subtree.
[QEMU serial options](https://www.qemu.org/docs/master/system/invocation.html)

An independent host AT-SPI client reads the fixture's Unicode text, button
roles and disabled state. It checks fitted bounds at 2× and observes the
text change through the same AT-SPI object after native keyboard input.
The password field is absent. Guest window exit invalidates the old objects,
then reopening exposes a new subtree. Console disconnect retires those
objects and leaves the last video frame visible.

Forty CTests and four native runs passed. The runs cover Release, ASan/UBSan
with leak detection, guest accessibility with both clipboard directions,
and the baseline graphical guest. Ten transport cases, seven worker cases
and six reset-receipt tests passed in each of two build profiles.

The guest initramfs SHA-256 is
`9294a9863d01893c9dcb5b0a831d3f3c9f1cf1b5be140271a26c3afbb024040c`.
The archive `nexaui-qemu-ax-live467.tar.gz` SHA-256 is
`1ac384d8762e3347afdf021ea3dfc1d3a2c131b4e1b45d1f3a41ab752b205fc0`.
Its manifest records 37 verified members, including the matching source and
the exact guest overlay.

Several windows, other window geometries, guest action forwarding,
screen-reader acceptance and Aetheri compositor adoption remain open.

## Native QEMU Explorer pointer, 2026-11-18

The tested source is `ca0cd7b7eb86de0420fa930962075fa4a55bfa05` on installed
SDK `266404bf1743248f16eb289c0a1c0d99701cc395`. The graphical
`--guest-pointer` profile requires the Linux guest to report a PS/2
Explorer mouse with two wheel axes and two side buttons. It uses the
existing host focus gate, fitted bounds and private QMP input path.
The adapter accumulates sub-tick movement during the current input
session and limits each native event to seven ticks per axis. The owner
sends separate QMP input commands for the two axes.
[QEMU 11.0.3 PS/2 packet handling](https://github.com/qemu/qemu/blob/v11.0.3/hw/input/ps2.c)
[Linux 6.18 PS/2 input decoding](https://github.com/torvalds/linux/blob/v6.18/drivers/input/mouse/psmouse-base.c)

An independent native peer sends four Wayland wheel directions and both
side-button presses from a private Weston seat. The guest's `evtest`
trace reports all four signed wheel changes. The host escape releases
both held side buttons. After re-entering, movement into the letterbox
releases a held side button again. Host wheel input and physical button
releases after exit produce no additional guest events.

Forty CTests passed across Release, ASan/UBSan with leak detection and the
default build. Both instrumented and Release native pointer workflows
passed. The VGA workflow passed with its three-button profile. A combined
run first passed its pointer checks and timed out waiting for the GTK
startup marker. An unchanged retry passed the combined pointer and
accessibility workflow. The evidence includes both outcomes.

A focused C probe covers fractional ticks, saturation, focus retirement,
and rollback when the input queue is full. Seven owner protocol cases and
six reset-receipt tests passed in each of two profiles. The audit retains
the exact source and 27 verified evidence members in
`nexaui-qemu-pointer468.tar.gz`, SHA-256
`1e5f6ea351973d033f836ef61980dad990db7511c0f25e08efd76b99cb19d972`.
The guest uses the same kernel and initramfs as the accessibility relay.

The verified scope reaches the guest Linux input device. Guest application
reactions, physical mice and touchpads, other guest operating systems,
and Aetheri compositor acceptance remain open.
