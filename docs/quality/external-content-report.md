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
