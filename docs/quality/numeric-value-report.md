# Copied numeric values and original Settings AT-SPI assignment

SDK **0.1.33**, source **`1e00a6f4f3a0ab3a7da1aa35a36f02679421366b`**.
The public API and initial transport were introduced at
`d8894949299f4de915fb487493c4f42df6be1ad0`; the final source also prebuilds bounded
native failure replies and adds the two-window cancellation regression.
Original Settings consumer inputs are frozen at that earlier `d889494` revision;
every consumer-owned NexaUI input is byte-identical at the final SDK source.
Original Aetheri source: **`793459008d2fe579fee92efbe2d525ea37c5f4e2`**;
minimal legacy model: `7bbe7042ecd48582b11ef7053a4dbeade6b02351`.

The earlier [ordered-step checkpoint](value-steps-report.md) made native
increment/decrement affect the original volume intent. This checkpoint adds
typed numeric reads and absolute assignment to built-in slider/drag values,
including Linux's standard `org.a11y.atspi.Value`. Capability strings alone do
not create an application numeric model. The [audio composition](settings-audio-report.md)
retains its own older sources and evidence.

## Contract and ownership

`nx_semantics_get_numeric_value` copies the actual minimum, maximum, increment,
value, control kind, writable flag and completed-frame/context tokens.
`nx_semantics_request_numeric_value` admits one finite in-range absolute value
against that unchanged snapshot. It shares the existing 64-entry action queue
and admission order. Pointer/keyboard input precedes queued SET/step operations;
ordinary snapping still applies. No focus or synthetic click is introduced.

Submission must retain context, ID, kind, role, parent, overlay, enabled scope,
range/step and previously observed caller value. Disabled/modal-blocked controls
remain readable but reject writes. Omission, changed observations and retired
native leases cancel. Successful painting commits once; painting failure leaves
the caller value and request available for a same-frame retry. A pre-release
false-applied acknowledgement on disabled retry was corrected before publication.
See the [public C17 example and error contract](../concepts/accessibility.md#copied-numeric-values).

The Linux bridge exposes minimum/maximum/increment/current/Text properties and
editable state. A native setter receives its reply only after actual widget
assignment or cancellation. Generation/incarnation/context/frame leases guard
pending and forwarded work; the existing 64-slot/8-MiB native budget and two-second
deadline remain. Success and failure replies are prepared at admission, so
retirement needs no new error-message allocation. Value changes emit an actual
double-valued `accessible-value` property notification.

SDK admission/assignment does not acknowledge an audio device operation. The
original audio owner still decides intent admission, pending/observed/unknown
state and loss fencing. No app model, authority, worker or device is owned by the
accessibility adapter. Existing public declarations/layouts are unchanged; the
new snapshot and two public functions are additive. The private Linux guarded
entry is not a new portable API promise.

## Executed local evidence

All formal runs use UID65534 in Alpine-Dev/WSL, Clang22.1.3, C17 with extensions
OFF and warnings as errors, Wayland-only/X11OFF and VulkanOFF. The three SDK
profiles are Release static, Release shared and Debug `-O1` with ASan/UBSan,
frame pointers and active LeakSanitizer. The original consumer uses the matching
installed prefix, strict conversion/shadow warnings and the same sanitizer policy.

| Evidence | Actually executed scope |
| --- | --- |
| SDK CTests | 15 core regressions plus one real private-Wayland numeric workflow per profile: **48** total |
| Installed SDK packages | **3** external C17 consumers exercise the new public getter/setter against installed libraries |
| Original Settings consumer | 10 CTests per enabled profile plus current/legacy default-OFF shared 5/3: **38** total |
| Original native tasks | **12**: three profiles × numeric/legacy-step client × full loss/recovery or pending-close |
| Native total | **15** workflows, including the three already counted as SDK CTests |
| Source binding | **1,100** SDK Git inputs, **50** consumer Git inputs, **305** actual positive objects (225 SDK/package +80 consumer) |
| Generated inputs | **36** files reproduced byte-for-byte from their recorded Wayland scanner/XML |
| Pictures | **16** complete headless original-view captures; eight cross-profile and four published-SDK0.1.32 reference equalities |

Thus **86 local CTests**, three installed consumers and twelve additional native
tasks passed; do not add the three native CTests again. Full commands, hashes,
compiler flags, XML results and frame transcripts are in the
[SDK receipt](numeric-value-local.json), [original-consumer receipt](numeric-value-settings-local.json)
and [raw archive ledger](numeric-value-archive.json).

Public core regressions cover cleared error outputs, stale context/frame,
nonfinite/out-of-range values, duplicate and mixed 64-entry admission, ordered
INC/SET/DEC, keyboard-before-SET, no replay, changed range/step/model/identity,
disabled ancestors, overlays, omission and actual drag values. Linux-private
regressions additionally cover retained guards, completion/release exactly once,
revocation/destruction and allocation failure followed by normal, disabled or
revoked same-frame retry. Public API tests run on Windows/macOS too; no private
DLL export is required for their test linkage.

The generic native fixture uses two actual Wayland windows and an independent
system libatspi/raw D-Bus peer without SDK linkage. A same-sender read barrier
proves the setter was admitted before the application changes. Seven actual
cancellations cover disable, range, observed value, reparent, control kind,
unpublished newer frame and provider retirement after forwarding. The second
window keeps the native hub alive while the first retires. Old endpoints become
Defunct where appropriate, the withheld value never replays, and both windows
close normally. Each final transcript ends `NUMERIC_LEASE_CLOSED 5 23`.

The original peer reads real Value properties, uses the unmodified libatspi
setter for61, checks invalid bounds/NaN/infinity and disabled writes on the wire,
then observes actual original owner pending60, acknowledged61, mute/default,
loss/recovery and permanent fencing. Copied old bus/path endpoints reject late
writes after loss/recovery. Pending-close deliberately ends before assignment
acknowledgement; full mode verifies raw double61/60/61 events. Original35-second
peer/60-second compositor deadlines, 100-frame and128-KiB transcript guards remain.
The generic new fixture has its own256-frame cap and existing outer deadlines.

The optional original host uses17 privately extracted systemd259.10-1.fc44
headers, pinned RPM SHA256
`5084b02d680e990d022734f8083aa40c5f882ef68d9f8e8710cbaa2eae511711`.
It adds no systemd/PipeWire link to the SDK. The shared SDK introduces no
GLib/GIO/systemd/PipeWire/SDL/X11 dependency. All543 previous ELF definitions
remain, with two new public and one private guarded numeric definition.

The current Aetheri HEAD observed during binding was
`004573b0138cab0401a0129b3c3d7b48ef76dc52`: all26 consumed original inputs are
unchanged. This is read-only Git evidence, not an update to its source, SDK pin,
defaults, builds or processes. Aetheri continues independently.

## Negative controls and retained failures

The final independent Release peer rejects the exact previously published
SDK0.1.32 original-host binary at `numeric_values(volume,60,true)`, exit1.
That intended early negative kills its child; it proves detection of absent
Value support, not successful teardown or sanitizer acceptance.

The unchanged final guard regression also rejects the actual earlier unreleased
Value prototype: failed painting followed by disabled retry incorrectly reported
applied despite unchanged model0. Configure/build return0, the regression
returns1 after cleanup, with no ASan/UBSan/LSan diagnostic. This is a prototype
defect; released0.1.32 had no absolute Value implementation.

Actual system libatspi2.60.5 limitations were diagnosed before adapting the peer:

- Its setter unconditionally unrefs a null error reply after an invalid write;
  an actual GDB stack records SIGABRT at `atspi_value_set_current_value(-1)`.
  Raw D-Bus now verifies exact refusal errors; normal valid assignment still uses
  the unmodified library. See the [versioned setter source](https://raw.githubusercontent.com/GNOME/at-spi2-core/2.60.5/atspi/atspi-value.c).
- Its event decoder delivers a `gint` fallback for the double variant. The final
  peer requires the real libatspi notification and separately validates exact
  wire signature and double values. The failed double-GValue assertion and
  actual diagnostic events are retained; no system library is patched. See the
  [versioned event decoder](https://github.com/GNOME/at-spi2-core/blob/2.60.5/atspi/atspi-event-listener.c#L1152).
- Defunct libatspi proxies may dispose their app pointer. The final peer copies
  endpoint strings before retirement, then sends the late wire request. The
  earlier helper failure occurred before sending and is retained separately.

Earlier preparation errors, source-count assertion, private fixture compile
errors, diagnostic link-wrapper error, generated builder syntax error and the
first receipt helper's incorrect pending-close event expectation remain in the
raw history. They are not product passes. The initial DrvFS copy failed before
compilation; its tool-truncated traceback is explicitly not represented as a
complete retained log. Byte-only export subsequently avoids unsupported metadata
copying. No failed runtime was relabelled as a sanitizer success or retried only
because an observation timed out.

## Exact-source CI and remaining work

[CI42696490764](https://github.com/Lulus792/NexaUI/actions/runs/42696490764),
attempt1, passes all seven required jobs on final source1e00a6f. Windows
static/shared executes147/146 tests; macOS123/122; Ubuntu271+1/269; sanitizers
execute271 in each driver-discovery order. Package, close-request and source-tree
consumers pass. The public numeric regression executes on all platforms; Linux
also executes the guard and real native cancellation tests, twice under
sanitizers. No CTest is skipped. Optional Windows-ASan and fuzz-campaign jobs are
skipped and are not counted as executed. The [CI receipt](numeric-value-ci.json)
binds seven complete decoded job logs; the
[CI/documentation supplement](numeric-value-ci-archive.json) preserves them.

UIA/AX numeric bindings, other numeric kinds, complete assistive/human acceptance,
actual audio-session/device admission, audibility/persistence, actual Aetheri
compositor Q04, canonical-font/IME, physical scaling and end-to-end performance
remain open. The original Editor held-EOF native failure remains unresolved;
this checkpoint neither retries it nor infers its cause. Further packet6
compositions and packets7–8 remain. All103 catalogue IDs/statuses are preserved.
No1.0 release, broad adoption approval or new performance ranking is claimed.
