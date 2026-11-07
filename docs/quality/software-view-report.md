# Native selected-app observations — 2026-11-07

Consumer source: `33b9a45cd51c679d743dc4c4a8ae2e4afb5abff8`.
Original Aetheri: `46e2a21ef979d9a76db2a8487d1de167a6a79ef7`.
Installed NexaUI SDK: `c4f467d85b0147944d2456f5e7602a39ac18e579`, 0.1.23.
Scoped requirements: D07/L04/L05/L11/I01/I05/T11/A02/A06/P04/P05/P07/Q02/Q04/Q08.

[Example34](../../examples/34_aetheri_software/README.md) now presents the original
selected-app observations through an ordinary native Wayland software-rendered
window. A virtualized list, local filter/draft and read-only detail surface reuse
existing NexaUI controls, text layouts, semantics, field clipboard/IME adapters,
worker channel and native wake FD. No SDK API/ABI/version or independent Aetheri
source/pin changes. This is the software composition's selected-install window,
not a complete inventory or management/admission service.

## Presentation and identities

The trusted host supplies a protected Root source, recipient account/UID and full
selected install IDs. The worker alone queries the original catalogue; the view
receives immutable observations, not SQL or administrative capabilities. Original
metadata retains source epoch/boot/runtime, recipient, install/component/publisher/
scope/generation/artifact IDs and original revisions/gate state. Details display
these complete values. Gate metadata is explicitly no launch ticket, permission
or current-process observation. Per-target ENOENT remains an ambiguous unavailable
eligibility observation, with no stale successful-install fields and no inferred
removal, denial or complete absence. All SQL writes belong to private test-fixture
preparation; the view/host issue no writes, migrations, launches or effects.

Filter/Read/Close, Tab/Shift-Tab, full logical Up/Down/Home/End, row Select/Enter,
Back/Escape and PageUp/PageDown compose the existing controls. A 128-byte validated
UTF-8 filter searches component, observation label and full ID. Back preserves the
draft without applying it. Filter/details/resize do not submit backend work;
only startup and explicit Read query again. Selection stays a full install ID.
Small windows retain state and expose Close; larger windows restore focus.

New observations and mode/filter changes unpublish and replace the semantic
context before freeing it. Queued old-context actions cannot retarget a newer
observation or overwrite a retained filter draft. Refresh restores full-ID focus
after measuring visible/overscan heights, rather than trusting initial estimates.
The native host publishes completed frames, waits with `nx_window_poll_with_fd`,
and retires the UI/renderer/window before stop/join/drain and font release. The
existing bounded worker still has no UI object or callback. Original I/O has no
deadline; current native tests do not hold an original query during window close.

## Actual local checks

Static Release, shared Release and GCC ASan/UBSan consumers each pass **4/4
CTests**. The previous model and worker tests remain, including nine unchanged
worker Root schemas per profile, separately labeled model runtime replacement and
the declared maximum model. The new real-font view test uses private Root v2 and
v10 observations, with exact Root/User database bytes/rows unchanged. Each tests
three styles: 800×600 dark/16px/1×, 360×300 German/high-contrast/32px/2×, and
480×360 light/16px/1.25×. That is 18 style/schema/profile cases, plus six declared
100,000-target view cases. The maximum view checks full-ID end navigation/filter,
bounded mounted semantics (fewer than 200), measured-row work and failed-allocation
rollback. It is synthetic selected metadata, not 100,000 real installed apps,
original I/O throughput, allocator peak/RSS or an accepted performance budget.

The view test checks full successful/unavailable details, overflow-dependent page
navigation, draft/selection retention, stale field/action queues, full-ID focus
after refresh, reading/error states, invalid/oversize filters, compact restoration
and explicit Read/Close intents. Its 144 final PPM captures bind exact pixels to
the three tested styles/densities. Six converted images were visually inspected:
[normal list](software-view-images/800-list.png),
[light list](software-view-images/480-list.png),
[large contrast list](software-view-images/360-list.png),
[detail start](software-view-images/360-detail-top.png),
[detail end](software-view-images/360-detail-bottom.png),
[retained filter draft](software-view-images/360-filter.png).
Normal/light lists are readable; large labels wrap, long details/help scroll, and
the last detail text remains reachable. These are headless rendered pixels, not
physical-display or human accessibility acceptance. Narrow 200% toolbar labels
wrap across lines; broader visual/human acceptance remains open.

Six native cases use the frozen original headless Aetheri compositor and a separate
OS libatspi process: three installed profiles × German/large/contrast/controlled
IME or normal English/light. Each passes **10 independent OS checkpoints**. The
peer observes actual list items/full IDs, selects the final unavailable target,
reads the complete read-only Text interface with no caret/selections/editability,
and checks detail scrolling and Back focus. Old row/detail/field references retire
across mode/observation replacement and refuse further actions. Read replaces the
observation. Native keys edit a filter; controlled v3 IME preedit disables Apply,
commit produces exact `xé`, and native Enter applies/clears it. Full-ID selection
survives filtering. The host exits through the focused Close button. Every host
and peer exits 0, all owned children are reaped without forced kill, and exact
Root/User database bytes/rows remain unchanged. Exactly two requests/observations
are recorded; local inspection/filtering causes no extra backend call.

Process maps bind shared hosts to the selected installed NexaUI library and show
no SDL/X11/EGL/Vulkan presentation stack. The peer links real OS libatspi/GLib
without NexaUI. Its explicit compiler command uses the carrier's real headers and
versioned libraries: the full pkg-config dependency chain is unavailable, so the
optional CMake peer target is not falsely counted as configured. Compositor input
hash is `631ec1740d9de6daac790d2c29351df7ccb2cabce936ac6ab35ba59b8cb71c75`;
this is a pinned test compositor, not current compositor/service adoption. The
borrowed Liberation Sans font hash is
`76d04c18ea243f426b7de1f3ad208e927008f961dc5945e5aad352d0dfde8ee8`;
the font is not redistributed. Sanitizers use leak detection/halt-on-error with
no final diagnostics. No new SDK core-test run is claimed.

## Failures and source binding

The first view test incorrectly required scrolling when normal-font details fit;
it now compares against measured overflow. The following run exposed a real
large-text focus loss after refresh. A diagnostic showed focus1004 normally and
focus0 at 32px/360×300; reapplying the full-ID anchor after measurement fixes it.
All three styles then passed, while the maximum view exposed double-versus-float
convergence below the scroll coordinate's representable precision. Comparing in
the actual float coordinate fixes that case. Initial inputs/builds/failures and
the passing final matrix remain archived.

Two private defects compile and fail intended checks: removing the measured
full-ID anchor recreates the large-text focus failure; reusing the old context
lets a queued old field request overwrite the retained draft. The latter fails
the exact draft check. No failed mutation is counted as passing sanitizer/native
acceptance. An uncompiled first source export had a test call corrected to the
existing UTF-8 span signature before its first build.

All 22 final source inputs match `33b9a45` after only CRLF-to-LF normalization.
All 21 executed inputs remain unchanged from the complete tested export; README
alone was updated after execution. The immutable original checker still verifies
67 source inputs. [Local receipt](software-view-local.json) retains commands,
compile flags, binary/library/source hashes, native process maps, cases, logs,
images, negative results and failures. Prior main `b3b3127` has all seven required
CI jobs successful and two optional skips; [exact-run API receipt](software-worker-ci.json).
This is API status evidence, not downloaded raw CI logs. Optional example34
workflows remain local evidence separate from root CI.

The [archive receipt](software-view-archive.json) identifies the independently
verified local artifact and complete manifest. The collector's own live log and
terminal journal entry are outside its frozen boundary. Earlier artifacts and
Aetheri's independent changes remain intact.

The accepted artifact is `D:/NexaUI/out/nexaui-software-view-20261107-v2.tar.gz`,
47,178,798 bytes, SHA-256
`0970627f07900d3563fe2d8ebdc5faf5596ea1de5cc78082c0eb6a9f54c7d81b`.
Independent local verification checks all 3,413 file hashes/sizes/modes, 37
symbolic-link targets and 1,274 directory modes. The first archive is retained:
its collector emitted some shared-inode AT-SPI inputs as tar hardlinks although
the manifest described regular files, and the complete verifier rejected it.
The v2 collector clears tar's inode memo before adding each entry, storing each
regular manifest entry as regular bytes while preserving unfollowed symlinks.
No consumer code, test, build or native result changed for this packaging repair.

## Remaining scope

Next compose exact observed artifact/component/publisher agreement with the
existing verified manifest/dependency reader. Full inventory, ownership/current
process/resource/permission/device observations and install/update/removal effects
still require actual owner/service contracts. Native software-specific held-query
close/wakeup, foreign clipboard ownership/late completion, extended navigation and
resize/output migration need their own composed checks; prior Recovery tests do
not automatically prove them here. Trusted login/installer/recovery hosts, the
rest of packet 6, packets 7–8, declared performance budgets and physical/human
screenreader/input/display acceptance remain open. All 103 requirements and all
planned Aetheri surfaces remain in scope.
