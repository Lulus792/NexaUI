# Original Settings audio-owner composition — 2026-11-09

Consumer **`a8ef31de8f6b0afb476221d6840255e553930321`**; installed SDK
**0.1.32**, source **`f1c96419a33ac00e14ab14ec8c7f93cfec2aea93`**.
Original Aetheri **`564dc2c7468826cbe24e0143e661ab90681778f7`**;
minimal legacy model `7bbe7042ecd48582b11ef7053a4dbeade6b02351`.
This follows the [original-view checkpoint](settings-original-view-report.md).

## Composition and ownership

Example21 adds default-OFF `NEXAUI_MODEL_BUILD_ORIGINAL_AUDIO_ENTRY`. Its caller
must enable the original view, use installed NexaUI>=0.1.32 on Linux, and supply
an existing `AETHERI_SETTINGS_AUDIO_HOST_TARGET` from the same original source
generation. The `aetheri_original_audio_entry` static target compiles unchanged
original `nexaui_main.c` with `AETHERI_SETTINGS_AUDIO_HOST` and
`AETHERI_SETTINGS_NO_MAIN`, linked to the original view/model and supplied host.
It adds no new process loop or replacement audio state machine. See the
[application build/ownership instructions](../../examples/21_aetheri_model/README.md).

The application independently admits its session, pushes its owning thread's
GLib context, opens the original audio host and calls the original
`ae_settings_nexaui_run`. Original prepare/finish/abort operations integrate the
host's descriptors/deadline into the existing SDK window wait. The temporary
model is unbound before its stack lifetime ends. The application retains its
session/context and handles deferred owner retirement after the window returns.
The ordinary executable keeps its unavailable-audio behavior. SDK/default-OFF
builds acquire no GLib, systemd or PipeWire dependency.

The standalone [controlled fixture](../../tests/aetheri_settings_audio/README.md)
compiles unchanged original entry, model, view, audio pump and GLib host. Only
the session-control observation/effect boundary is controlled. It starts no real
service, performs no admission and changes no actual device. Its inherited
`SOCK_SEQPACKET` channel is private test coordination, with no listening endpoint.
Owner callbacks run on the owning GLib context/thread.

The original application header includes systemd declarations. A private
header-only SDK contains 17 headers from Aetheri's manifest-pinned
`systemd-devel-259.10-1.fc44.x86_64.rpm` (718,475 bytes, SHA-256
`5084b02d680e990d022734f8083aa40c5f882ef68d9f8e8710cbaa2eae511711`).
Nothing is installed system-wide and no systemd library is linked. No fresh RPM
signature verification is claimed. Provenance and header hashes are in the
[local receipt](settings-audio-local.json).

## Executed checks

Alpine-Dev WSL, Clang22.1.3, C17/extensions OFF, UID65534. Consumer warnings are
errors. Debug-O1 ASan/UBSan/LSan retains leak detection and halt/stack traces;
no timeouts, frame guards, quotas, drivers or system settings are relaxed.

| Consumer profile | CTests | Retained objects | Result |
| --- | ---: | ---: | --- |
| Audio entry ON, Release/static | 10 | 21 | Passed |
| Audio entry ON, Release/shared | 10 | 21 | Passed |
| Audio entry ON, Debug-O1 ASan/UBSan/LSan | 10 | 21 | Passed |
| Default OFF, current original model, shared SDK | 5 | 10 | Passed |
| Default OFF, minimal legacy model, shared SDK | 3 | 7 | Passed |

Total **38 consumer CTests / 80 objects**, including the independent native-peer
objects. The [SDK prerequisite](value-steps-report.md) separately passes **39
CTests / 213 objects and three installed consumers**. Do not add preliminary or
capture reruns as distinct CTests. All **50 consumer Git inputs**, all 1,091 SDK
inputs, actual compiled objects/flags/libraries, generated protocols and complete
logs are hash-bound. The 26 original files remain byte-unchanged at independently
read Aetheri HEAD `a58b0cc3f65fed2bd42cac3b0d5d1694b2f21884`.

The unchanged original binding/events tests execute in all enabled profiles.
Events uses actual GLib I/O, timers, threads and descriptor retirement, retaining
the original 15-second deadline. The unchanged original view matrix retains its
separate density1/density2 30-second cases, each exercising 360 combinations
and subsequent interaction checks. Maximum64-output coverage also remains.

Three actual configure failures verify the public composition guards: audio
without the original view, no existing caller host target, and installed SDK
0.1.31 rejected by the 0.1.32 minimum. Both default-OFF profiles have no original
view/audio targets or GLib/systemd include flags.

## Native action, observation, loss and close

Each enabled profile passes two actual private-Weston/Xvfb tasks with a private
D-Bus/AT-SPI registry: **six native tasks**. The independent system-libAT-SPI
peer links neither the model nor SDK. It navigates the actual original Sound
view and uses the slider's increment Action. This needs the
[0.1.32 numeric-step fix](value-steps-report.md); advertised actions alone were
insufficient in 0.1.31. Values are observed through `nexaui-value` attributes,
not a standard native numeric Value interface.

The full task verifies:

1. One volume request becomes PENDING; the observed volume stays60 and controls
   disable. A duplicate disabled action is rejected and the effect count stays1.
2. An explicit external observation acknowledges volume61. Mute then follows
   its own pending/acknowledgement sequence, followed by a default-output request.
3. Owner loss leaves the third request UNKNOWN, publishes the localized loss
   label and invalidates/rejects old control references. A deliberate recovery
   observation cannot clear the original model's permanent loss fence or replay
   the request. The original ordered AUDIO diagnostic after RECOVER remains
   fenced1; the effect count remains3.
4. Actual Close returns through the original window loop. Before owner disposal,
   the third command is still UNKNOWN and the unacknowledged default output
   remains unchanged. The original host closes and the controlled owner retires.

The second task closes while the first volume command remains PENDING. After
window return the owner still reports that one pending request; it is not
converted into success/cancellation. Both variants require clean final retirement.
Complete bounded transcripts retain the post-window and retirement markers.
Existing 35-second client and 60-second Weston limits remain, with 100-frame and
128-KiB transcript guards. This is original host integration with controlled
observations, not actual session/backend/device-effect acceptance.

There are **16 complete headless RGB captures**: four scenes in each 1× profile,
plus four ASan 2× scenes. Eight complete cross-profile pairs and four Release
pairs against the old 0.1.31 reference are byte-identical. Borrowed Arial SHA-256
is `b3658eadae55e682b5f69eb64c439c1ecc8f196c0bb8d4756d145d13bc86476a`;
font bytes are excluded from the archive. These are no new native/compositor
pixel or human design approvals.

## CI, raw evidence and retained failures

The [CI receipt](settings-audio-ci.json) records exact-source SDK/package jobs,
attempt history and complete decoded logs for
[run42673442896](https://github.com/Lulus792/NexaUI/actions/runs/42673442896).
The standalone original audio/native checks have the separate local evidence
above; generic SDK CI must not be described as running them.

Attempt2 passes all **seven required jobs**: Windows static/shared146/145,
macOS122/121, Linux static268 plus one lifecycle case/shared266, and268 tests
in each of two sanitizer driver orders. There are no CTest skips; two optional
jobs are skipped. Installed, close-request and add_subdirectory package
consumers pass on all six core jobs. The receipt binds all twelve complete
decoded logs across both attempts, including their distinct content hashes.

Attempt1 ended in workflow failure despite all five created jobs succeeding;
Linux/shared and macOS/shared were not created. GitHub's annotation was
`Internal server error. Correlation ID: a81b31c6-0b69-4330-a43b-58cb8b3390d8`.
The integration's targeted retry lacked access; an existing authorized Git
credential's targeted retry returned403, `This workflow run cannot be retried`.
A full retry was accepted on the same immutable source. It re-executes the
jobs; the first attempt's five complete logs remain independent evidence.
No workflow, credential scope, driver or source repair is attributed to this
orchestration failure.

The [verified raw archive](settings-audio-archive.json) is
`nexaui-settings-audio3-20261109.tar.gz`: **231,530,015 bytes**, SHA-256
`904346ec2083f96f9c14079aa3d40a5ec3470040c2ed8d4996dc4030009664b0`,
**1,646 regular files / six symlinks**, every member checked. It retains source
exports, input archives, binaries/objects, commands/logs, captures, header SDK,
old-SDK failure and compiled parent-identity negative. Later CI/docs are retained
separately in the [CI supplement](settings-audio-ci-archive.json).

Retained preparation failures include the actual no-submission 0.1.31 native
case, unavailable Python `_zstd` extension (the existing libzstd decoded the
pinned RPM instead), a guessed prerequisite target name, CMake line-wrapped
diagnostic matching and missing package compilation databases. Final object
binding uses actual Ninja C compiler rules; no production change was made for
those verifier corrections. Intermediate nested-script escape warnings and
empty generic Ninja databases remain separate failed preflights. Earlier original
view's unoptimized-ASan timeout and Editor held-EOF native failure remain open
in their own reports, not retroactively passed by this step.

## Open catalogue gates

B06/B07/D07/P01/P05/P07/Q08 gain scoped composition/wait/lifetime evidence;
L03/I01/A01 gain the typed numeric-step path. **All 103 IDs/statuses and remaining
packets6–8 are preserved.** Active Aetheri source, builds, pin, defaults and
processes are untouched; its implementation can progress independently.

Still open: actually admitted original session/backend and real device effects,
audibility/persistence, actual Aetheri compositor Q04, standard native numeric
Value/SetValue and other numeric control kinds, IME tasks (this original view
contains no editable field), canonical fonts/fallback, physical input/display,
human screenreader/design review, declared end-to-end performance budgets and
normal owner-launch/adoption. Other Editor/terminal/content/VM compositions and
the rest of the full catalogue remain work, rather than implied acceptance.
