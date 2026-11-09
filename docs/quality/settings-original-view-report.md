# Original Settings/Sound view and ordinary Wayland host — 2026-11-09

Consumer: **`a45b55349d44c64babde2d4da5f1e14559c9c640`**.
Original Aetheri: **`509766ed0c4bf2edcb1fc15b8305c34133aba2a8`**;
minimal legacy model: `7bbe7042ecd48582b11ef7053a4dbeade6b02351`.
Installed SDK: **0.1.31**, production source
`9f0b2294d04d7f5fdd63d3fe5c118b2bd222eade`.

## Composition and ownership

Example21's default-OFF `NEXAUI_MODEL_BUILD_ORIGINAL_VIEW` compiles the supplied
original `nexaui_view.c`, unchanged original view test and, on Linux, original
`nexaui_main.c`. It shares the original model/text helper from the
[compatibility checkpoint](settings-model-compatibility-report.md). It introduces
no replacement Sound model, controls or native process loop. See the
[build and adoption instructions](../../examples/21_aetheri_model/README.md).

The original host presents Displays, Appearance, Language and Sound with its own
chrome, keyboard handling, software renderer and semantic publication. The
ordinary executable does not define `AETHERI_SETTINGS_AUDIO_HOST`: Sound correctly
reports unavailable audio. Admission, the audio session, observations and effects
remain original application responsibilities. No GLib/PipeWire dependency is
added to NexaUI and no service is started or reconnected.

The new `aetheri_original_sound_test` exercises all 64 admitted outputs and a
511-byte accessible description. Every output name is checked. Keyboard intent,
no optimistic device changes, queued intent across replacement at German/200%,
once-only dispatch, reordering with stable IDs, explicitly supplied outcome and
permanent owner-loss fencing are checked through the actual original view.
These are deterministic model inputs, not independently observed device effects.
The unchanged original tests additionally cover volume drag/release/cancel,
keyboard volume, mute, unavailable controls, amplified observations, pending
results and replacement serials.

SDK production files under `include`, `src`, `platform`, `renderers`, `cmake` and
the root CMake file are byte-unchanged from the named SDK source. There is no
API/ABI/version/backend dependency change. Aetheri's active source, pin, defaults,
builds and processes were not modified.

## Executed checks

Host: Alpine-Dev WSL, Clang22.1.3, C17/extensions OFF; tests run as UID65534.
Consumer flags include `-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror`.
Borrowed Arial SHA-256:
`b3658eadae55e682b5f69eb64c439c1ecc8f196c0bb8d4756d145d13bc86476a`.
Font bytes are not bundled; canonical Aetheri font acceptance remains open.

| Profile | CTests | Retained objects | Result |
| --- | ---: | ---: | --- |
| Original view ON, Release/static | 8 | 14 | Passed |
| Original view ON, Release/shared | 8 | 14 | Passed |
| Original view ON, Debug-O1 ASan/UBSan/LSan | 8 | 14 | Passed |
| Default OFF, current model, shared SDK | 5 | 10 | Passed; no original-view targets |
| Default OFF, minimal legacy model, shared SDK | 3 | 7 | Passed; no text/audio module |
| Unchanged SDK, Debug-O1 prerequisite | 10 | 67 | Passed |
| Last-output omission compiled into original view | Negative | 4 | Detected, exit1; no sanitizer diagnostic |

Total: **32 consumer CTests**, plus **10 SDK prerequisite CTests**. Consumer
objects total 59; the SDK prerequisite has 67 and the compiled negative 4. Six
independent native peer executables are recorded separately. ASan leak detection
and UBSan halt/stack traces remain enabled. The SDK retains its established
warning policy: global `-Werror`, with Wall/Wextra/Wpedantic on the core.

The original matrix retains Aetheri's separate density 1/density 2 registration
and original 30-second limit per case. Each executes 360 theme/language/text-size/
viewport/page combinations, followed by the original interaction checks.
All 42 exported Git inputs match their named commits; all 16 current original
files remain unchanged at the independently read Aetheri HEAD `509766e`. Every
retained object binds to actual source/compile flags. Six generated Wayland C
files and six headers reproduce byte-for-byte from the recorded system XML and
scanner. Configured but unbuilt targets are excluded. See the
[full source and execution receipt](settings-original-view-local.json).

## Native input, accessibility and pixels

Each enabled profile passes three private-Weston/Xvfb tasks, **nine native tasks
total**, using a private D-Bus registry and independent system libatspi peers:

- AT-SPI actions change appearance, text 200% and German, open Sound, observe its
  localized unavailable-audio label/role and original model page 3, then close.
- Actual virtual-seat keyboard input changes the same groups, reaches the
  localized Sound navigation button and exits through Escape. A separate AT-SPI
  observer checks focus/checked states; the existing wrong-focus negative must
  fail in each run.
- With the bridge disabled, the original window renders its unchanged model,
  remains undiscoverable and closes normally on SIGTERM. This is an executed
  unavailable-state check, not a skip.

The opt-in Sound extension reuses the existing
[candidate peers and keyboard driver](../../tests/aetheri_settings_candidate/README.md).
Older snapshots retain their original task. Peers link no NexaUI/Aetheri model;
external system includes/runtime dependencies remain test-host dependencies.

There are **16 full headless RGB captures**: four Sound scenes in each 1× profile,
plus four in the instrumented 2× run. All four 1× scenes match byte-for-byte between
static/shared and static/ASan, **eight complete image pairs**. Capture reruns are
not additional distinct CTests. Dark, muted high-contrast, German/light 200% and
focus-revealed controls were visually inspected: large text scrolls and the mute
control is visible when focused. Their service-confirmed/rejected strings use
deterministic test observations. This is not native/compositor-pixel, physical,
spoken-screenreader or human design acceptance.

All seven required exact-source [CI jobs](settings-original-view-ci.json) pass in
[run42663662547](https://github.com/Lulus792/NexaUI/actions/runs/42663662547):
Windows static/shared146/145, macOS122/121, Linux268 plus a separate lifecycle
case/266, and268 tests in each of two sanitizer driver orders. There are no CTest
skips; two optional jobs are skipped. Installed, close-request and add_subdirectory
package consumers pass on all six core jobs. These are SDK/package checks; the
standalone original Settings/Sound and native tasks have the separate local
evidence above. Complete decoded logs and final metadata are retained in the
[CI/supplement archive](settings-original-view-ci-archive.json).

## Failures retained and remaining work

An unnecessary generated entry point caused a duplicate-main link failure;
the original host already owns `main`. An unsplit two-density ASan probe exceeded
60 seconds. Splitting as the original project does preserved its 30-second
per-density limit, but **unoptimized ASan 2× still timed out**. The passing
Debug-O1 instrumented profile is a separate result, not a repair or pass for
that unoptimized timeout. No deadline or sanitizer was relaxed.

Preparation failures also retain a notice-path mistake, two wrong prerequisite
target names, old widget aggregate-initializer warnings from extra global flags,
external AT-SPI header diagnostics and generated-protocol binding mistakes.
The final peer command uses system include paths, as imported CMake targets do;
application warnings remain errors. The retained diagnostic regenerates all 12
actually built protocol files. These failures are not successful tests.

The [verified raw archive](settings-original-view-archive.json) retains frozen
exports, binaries/objects, commands, complete logs, fixture outcomes, all captures,
preparation scripts/failures and font hashes; font files are excluded.

B06/B07/D07/Q08 gain this optional installed original composition, with scoped
supporting I01/L03/P05 interaction/lifetime evidence. All 103 requirement IDs and
statuses, and remaining packets 6–8, are preserved. Aetheri can evaluate this exact
ordinary Settings consumer independently of other app development or pin changes.

Still open: admitted original audio host/service integration and actual effects;
actual Aetheri compositor acceptance (Q04); canonical fonts/fallback; IME tasks
(this view has no editable field); physical input/display and human screenreader/
design acceptance; declared end-to-end performance budgets and normal owner-launch
adoption. Other terminal/editor/content/VM compositions and the earlier original
Editor held-EOF native failure remain separate open work.
