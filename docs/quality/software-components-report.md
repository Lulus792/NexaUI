# Observed app to original manifest/dependencies — 2026-11-07

Continuation: source `de18e96e57a3bd0cbdedf2042fbfe1eec8ed4398` integrates
[declared file/action inspection](software-contents-report.md) into this exact
observation binding. The results and remaining work below retain their original
`865f21c` scope; later inspection does not establish actual ownership/process state.

Consumer source: `865f21c8f4a42ea62293b39f06f092863bbab99b`.
Software original: Aetheri `46e2a21ef979d9a76db2a8487d1de167a6a79ef7`.
Standalone manifest regression original: `1fb45c7c21f914a2893457132b609be50ef4280d`.
Installed SDK: `c4f467d85b0147944d2456f5e7602a39ac18e579`, NexaUI 0.1.23.
Scoped requirements: D07/L04/L05/L11/T11/A02/A06/P01/P04/P05/P07/Q02/Q04/Q08.

[Example34](../../examples/34_aetheri_software/README.md#exact-manifestdependency-join)
now joins an original selected-app observation to an explicitly selected original
canonical manifest. Details show declared version/target, file/action counts and
all zero-to-eight complete dependency artifact IDs. Missing metadata and failed
verification remain unknown/unavailable; installed/active dependency and ownership
state remains explicitly unknown. No complete inventory, publisher trust, payload
verification, launch/permission capability or administrative effect is inferred.
SDK API/ABI/version and independent Aetheri sources/pin are unchanged.

## Shared original reader and exact binding

The new application-private `amh_document` in example31 owns immutable canonical
bytes and the original `ae_fixture_verify_manifest` result. Its validated text
slices borrow that owned document until explicit free. The previous `amh_read`
path-list adapter uses the same bounded regular-file/no-follow/nonblocking loader;
`amh_project` retains its caller-byte contract. There is no second decoder/hash
implementation. Reads retain the original 131,072-byte limit plus an overflow
byte, close descriptors on error and never open a declared file/action/dependency.
Arbitrary filesystem completion still has no deadline.

Each optional manifest source is explicit owner configuration copied before the
worker starts. A GUI selection cannot choose a filename or retarget a queued job.
The worker first reads the eligible app observation. Only that row's original
artifact identity supplies the expected hash; source entries for unknown,
ineligible or unselected IDs cause no manifest open. Hash equality also requires
exact original component and full publisher identity agreement. A batch copies
the complete source/recipient/install/generation/gate/revision binding. UI lookup
against another UID, row, source or revision exposes no document even if bytes
still match. This binds historical observations; it creates no cross-query
transaction or future authority after file I/O.

App observation and optional metadata errors are separate. Original app-read
failure retains the old immutable view/binding. Missing manifest selection is
unknown metadata; malformed/wrong-hash/component/publisher or allocation failure
does not display a fabricated zero count. Zero declared dependencies belongs
only to an available matching verified manifest, and says nothing about installed
dependencies. Raw input, document, worker reply, batch and borrowed view lifetimes
are explicit; context retirement precedes metadata replacement, and every discarded
reply frees its remaining ownership once. The existing channel/wake/host APIs are
reused. Standalone example31 keeps its own original pin; example34 reuses the
existing 67-input check and adds three encoder/vector/spec hashes at its own pin.

## Executed checks

In static Release, shared Release and GCC ASan/UBSan, software passes **5/5 CTests**
and standalone manifest regression passes **4/4 CTests** each. Software retains
the prior model/worker/view checks. The new Root v2/v10 component runner requires
unchanged Root/User database bytes and rows after private fixture preparation.
Original encoder data reaches 32 paths, eight actions/eight arguments and eight
dependencies, including a 256-byte path/argument and 128-byte Unicode version.
Tests cover copied-buffer lifetime, empty dependency declarations, wrong hash,
hash-matching component/publisher contradictions, UID/revision/gate binding,
duplicate sources, allocation failure, symlink/FIFO/non-regular sources and FD
balance. A real worker receives overwritten caller configuration yet reads only
the two eligible configured sources, and moves its owned metadata once. Real-font
details preserve full IDs, unknown state, focus and complete page navigation.
These are real original C API/SQLite/verifier calls over synthetic metadata,
not installed payloads or production-service admission. The standalone manifest
native executable is rebuilt; this checkpoint's standalone regression is its four
CTests, not another claimed native hierarchy campaign.

Seven ordinary native Wayland/independent OS AT-SPI cases pass ten checkpoints
each: matching and identity-failure cases in all three profiles, plus one default
no-manifest CLI regression. The peer reads all eight exact 64-hex dependency IDs,
declared counts and unknown installed/ownership state. A canonical manifest whose
hash matches the original artifact row but whose component disagrees must remain
unavailable without a zero dependency count. Cases also retain full read-only
Text, old-reference retirement, focus/selection, Read replacement, native filter,
controlled IME where selected and ordinary Close. Each host/peer exits 0, no child
needs forced kill, original database bytes/rows remain unchanged, and only startup
and explicit Read post backend observations. Shared runtime maps resolve the
selected SDK; maps exclude SDL/X11/EGL/Vulkan presentation. The OS peer links real
libatspi/GLib independently, through a recorded manual command because the carrier's
full pkg-config chain remains unavailable. No new SDK core-test run is claimed.

The 24 final component PPMs cover normal dark/16px/1× and narrow German high-
contrast/32px/2× views at Root v2/v10 across all profiles. Four converted images
were inspected: [normal start](software-components-images/800-dependencies-top.png),
[normal end](software-components-images/800-dependencies-bottom.png),
[large start](software-components-images/360-dependencies-top.png),
[large end](software-components-images/360-dependencies-bottom.png).
Long identifiers wrap, the complete last ID is reachable, and the unknown-state
statement remains in the full read-only text. These are headless pixels plus
separate native transport evidence, not physical display or spoken-screenreader
acceptance. Borrowed Liberation Sans is hash-bound, not redistributed.

## Long-detail correction and retained failures

The first whole-document emergency wrap exceeded the native seven-second ASan
watchdog. An unchanged-host diagnostic with a 30-second observation window completed
correctly in about 14.066 seconds until the first detail VIEW observation. The
view now caches the existing grapheme wrapper per original paragraph and paints
only visible paragraphs. Complete original UTF-8 remains one immutable semantic
Text snapshot without a caret. Width/state/observation/metadata replacement clears
paragraph layouts; raster density uses the existing context API while retaining
logical layouts. Original bounds fit an 8,192-byte/256-paragraph example cache;
this is not a general document or memory budget.

A matching candidate diagnostic observed first detail in about 2.670 seconds;
the final seven-case matrix uses the original seven-second watcher. The two
diagnostics use identical original metadata bytes/font/SDK but different app
binaries, one process each. This is a scoped reproduction/correction, not CPU-only
timing, input-to-display latency, statistical benchmarking, an approved performance
budget or a general SDL comparison. The candidate's page-count checker correction
occurs after the first-detail observation.

The OS reader initially underestimated fractional PageDown steps by using an
outward-rounded integer viewport height. The diagnostic ended 11 pixels early.
It now computes a conservative step count from the rounding bound and still
checks actual final text extents and exact return to the start. No end-bound
assertion was weakened. Four copied compiled defects fail intended checks:
omitting original hash verification, component agreement, publisher agreement or
current observation binding.

Other retained attempts include the test helper's Linux `clone` name collision
and indentation warning, an original-encoder-rejected non-executable action fixture
(corrected to required0555), a restored errno include needed by the original
projector, an overlong private D-Bus socket path before host startup, and a negative
fixture copier refusing its test FIFO. Corrections preserve source guards; the
counterexample harness preserves symlinks and explicitly recreates its FIFO.
Final sanitizers run with leak detection/halt-on-error without diagnostics.

## Revisions and artifacts

[Local receipt](software-components-local.json),
[archive receipt](software-components-archive.json).
All 51 archived checkpoint inputs match `865f21c` after CRLF-to-LF normalization only.
The CMake/test matrix uses input5; the independent peer is rebuilt/executed from
input7 after its viewport correction. Actual product bodies match the final
revision. The comparison explicitly labels a later README explanation and removal
of one extra blank EOF line in the component test; neither is silently normalized
or described as an executed byte-identical input. All failures, source versions,
builds, commands, native results, images and diagnostic/counterexample outputs are
retained. Prior main `6bce490` has seven required CI jobs successful and two optional
skips; [exact-run API status receipt](software-view-ci.json). Optional example31/34
workflows remain separate local evidence; raw CI artifacts were not downloaded.
The current README separately clarifies logical paragraph caching versus raster
density; [exact post-source documentation diff](software-components-documentation.json).
This later clarification changes no implementation or archived test input.

The archive uses the prior corrected regular-file collector and does not follow
symlinks. Test FIFOs/sockets are explicitly excluded special entries, recreated by
the fixture recipes. Borrowed font bytes are excluded. Collector live log and its
terminal command-journal completion are outside the frozen boundary. Existing
artifacts and Aetheri's independent work remain intact.

`D:/NexaUI/out/nexaui-software-components-20261107.tar.gz`: 76,931,415 bytes,
SHA-256 `91222cc9d782164d6e9af3f9114849682e834897c7f978888747a3965e1c0098`.
Independent verification checks 5,838 regular files, 77 symlink targets and 2,281
directory modes. Forty special test nodes are explicitly excluded and named.

## Remaining scope

Compose the existing declared file/action inspection and actual owner-provided
dependency/ownership/current process/resource state next; declaration counts/IDs
must not stand in for complete installed inventory or effects. Software-specific
native held-source close/wake, foreign clipboard ownership and resize/output
migration still need composed checks. Device/service/permission/admission/trusted
host integrations, the rest of packet 6, packets 7–8, declared performance budgets
and physical/human acceptance remain open. All 103 requirements and all planned
Aetheri surfaces remain in scope; no 1.0.0 release is declared.
