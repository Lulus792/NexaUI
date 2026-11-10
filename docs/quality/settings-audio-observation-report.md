# Original Sound: unreadable observation after submission

Consumer **`3f51e2bf22359b02b01c5fb2854d69cd3e31c5c5`**, installed SDK
**0.1.33 / `1e00a6f4f3a0ab3a7da1aa35a36f02679421366b`**, original Aetheri
**`de4b61d6223df5e72f0c4345cd14f5e0b4d7808e`**; 10 November 2026.
This extends the [audio composition](settings-audio-report.md) and
[standard numeric Value](numeric-value-report.md) evidence. The 26 original
inputs are byte-identical to the prior numeric consumer export: this is a new
failure/lifetime check, not a newly implemented Sound view or device backend.

## Behavior and reproduction

The existing [standalone fixture](../../tests/aetheri_settings_audio/README.md)
adds `NEXAUI_AUDIO_OBSERVATION_FAILURE=1`, independently combinable with
`NEXAUI_AUDIO_NUMERIC_VALUE=1` and `NEXAUI_AUDIO_CLOSE_PENDING=1`. No public API,
SDK version, default build or production source changes. Original model, view,
entry, audio pump and GLib host compile unchanged against the installed package.
Only the already controlled session snapshot/effect boundary injects failure.
It starts no real service, admits no session and changes no actual device.

After the independent native peer submits volume61, the controlled owner retains
one PENDING command and observed volume60. The private owner-context channel
then makes snapshot reads fail. The peer requires:

1. The original stale-information label and the still-live original volume
   object, with observed value60 and disabled interaction.
2. Rejected volume, mute and output-choice actions, with no additional effect.
   Numeric mode also reads the standard range/current/Text and requires the
   exact `org.freedesktop.DBus.Error.PropertyReadOnly` setter refusal.
3. In the full task, restored readability changes the original label back to
   applying, while the same command remains pending and the value remains60.
   Restoration neither acknowledges nor replays the command. Only the separate
   explicit ACK changes the observed value to61 and enables interaction.
   The prior mute/default/loss/recovery checks then execute unchanged.
4. In the close-pending task, Close returns through the original window loop
   while reads remain unavailable. Before owner disposal, its one command is
   still PENDING, volume remains60 and readability remains false. Both variants
   require clean host retirement. Temporary unreadability is distinct from the
   full task's later permanent session-loss fence.

Use the existing private-Weston/AT-SPI command in the fixture README with the
additional environment variable. The independent peer continues to link system
libatspi/GLib, without the SDK/model. The existing raw numeric-error listener
still handles the documented libatspi2.60.5 error/event limitations.

## Executed evidence

Alpine-Dev, UID65534, Clang22.1.3, C17/extensions OFF, consumer warnings as errors.
Profiles: Release/static, Release/shared and Debug-O1 ASan/UBSan/LSan, using the
three existing installed SDK packages. Leak detection and halt/stack traces stay
enabled. Original 10/15-second binding/events CTest deadlines, 30-second view
deadlines, 35-second client/60-second Weston limits, 100-frame and 128-KiB
transcript guards remain unchanged.

Each profile passes **10 consumer CTests**, including the original GLib events,
model/view, density1/2 and maximum64-output checks: **30 CTests / 63 consumer
objects**. Each also passes four unavailable-observation tasks and four existing
baseline tasks: **24 native tasks, including 12 new failure tasks**. All complete
bounded host transcripts retain the original AUDIO transitions, effect counts,
actual failed snapshot reads and post-window owner state. There are no new
image comparisons, IME tasks, performance measurements or human approvals.

Two separately compiled private owner counterexamples fail with client exit1
under the original limits: ignoring unreadability is rejected at the stale
label; converting recovery into acknowledgement is rejected at the applying
label. Neither is accepted as an expected success, outer timeout or sanitizer
failure. Positive binaries and original sources are not replaced by mutants.

The [local receipt](settings-audio-observation-local.json) and
[source binding](settings-audio-observation-source-binding.json) record results,
1,791 consumer export inputs, 26 original inputs, 63 source/object pairs,
compiler preprocessing dependency responses and binary dependencies. Complete
native invocations and environment selectors are retained in the raw archive.
The three linked SDK library hashes equal their earlier numeric-value receipt.
No systemd/PipeWire library is linked; the existing explicit systemd header SDK
is reused. The borrowed font hash is recorded and its bytes are excluded.

Two verifier preflights remain attributable to preparation, not product failure:
Git-for-Windows archive applies CRLF while the three changed compiled exports
equal raw LF Git blobs; exact normalization records and actual LF overlays are
retained. The existing Alpine ninja substitute lacks `-t deps`; the final audit
uses unchanged compiler defines/includes with `-M`, retaining every response.
The negative receipt's mislabeled link-argument field is corrected explicitly,
with its original receipt preserved.

## Published CI

All seven required jobs pass on attempt1 of exact-source main
[run42762869909](https://github.com/Lulus792/NexaUI/actions/runs/42762869909).
The [CI receipt](settings-audio-observation-ci.json) checks complete decoded logs:
Windows static/shared148/147, macOS124/123, Linux static272 plus one presentation
repeat/shared270, and272 tests in each of two sanitizer driver orders. There
are no CTest skips; two optional jobs are skipped. These are generic SDK/package
checks, not execution of the standalone original audio fixture. Its local
30 CTests/24 native tasks remain separately attributed above.

The [CI supplement](settings-audio-observation-ci-archive.json),
`nexaui-settings-audio149-20261110.tar.gz`, is 254,571 bytes, SHA-256
`4c73265f0c23ad051211a4ad8a8afb2f381be64e6a97b74b3c1bf2e1e5ccd6e8`.
All 14 regular members, including its manifest, were verified. It retains all
seven complete logs, run/job receipts and the verification helpers.

## Archive and remaining gates

The [verified archive](settings-audio-observation-archive.json),
`nexaui-settings-audio146-20261110.tar.gz`, contains **77,595,077 bytes**, SHA-256
`3df5b15ac4029978e39a730266e8a5a4ffcdff97845ee0a065e37473aa379248`.
All **486 regular files / two symlinks**, including its manifest, were checked.
It retains source archives/actual overlays, build products, installed SDKs,
header SDK, helpers and complete local logs. External fixture runtimes and the
borrowed font remain separately required inputs. CI evidence is separate above.

I03/A01/A04/P05/P07/Q01/Q02/Q08 gain scoped native failure/retained-state evidence;
**all 103 requirement statuses and the remaining packets6–8 stay unchanged**.
Actual original session admission/backend/device effects, audible/persistent
results, Aetheri compositor Q04, canonical font/fallback/IME, other numeric
interfaces, physical input/display, human assistive/design review, performance
budgets and normal owner adoption remain open. Aetheri's source, checkout,
builds, pin, defaults and processes were not changed. SDK work stays independent.
