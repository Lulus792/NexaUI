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
