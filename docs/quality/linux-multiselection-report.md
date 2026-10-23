# Linux AT-SPI multiple-selection local evidence

2026-10-23. Candidate based on reviewed/published
`12ce9c5b549201773bb19f0ff6888437be6982fe`, in the separate
`codex/atspi-multiselection` worktree. This addition is locally implemented and
tested; independent review and full exact-candidate SDK CI remain required before
publication. No physical input, manual Orca or user-Mac acceptance is inferred.

## Implemented contract

The Linux provider exposes complete model-backed selected sets and
MULTISELECTABLE, with selected ordinals distinct from direct child indices.
Offscreen members remain logical members. Unbound sorting headers remain outside
the model catalog. Item add/remove and atomic whole-set SelectAll/Clear reach the
actual application models while preserving active ID and independent focus.
Disabled/required policies, model revision/membership, raw ancestor identity and
native lifetime are revalidated before commit.

Retained scalar C leases outlive detached bridges without retaining contexts or
models. Native pending and in-flight work together consume at most 64 slots and
8 MiB. Routing holds bridge/window storage across application allocator call-outs.
Snapshot staging completes context reads before the native allocator is called;
detach/destroy/nested publication cancels stale publication before attachment.
Strict incarnations apply when either old or new scope is multiple selection.
Existing Single-to-Single behavior stays covered by the previous native suite.

## Actual local executions

Environment: Alpine-Dev/WSL, Clang 22.1.3, system libatspi 2.60.5 and D-Bus 1.16.2.
Private Xvfb displays, private session/accessibility buses and real registryd are
created and cleaned by the existing fixture. Wayland uses a private Weston 14.0.2
X11 backend; the retained executable, shell/backend modules and helpers match their
original APK contents. No shared WSLg compositor or user desktop is used.

| Configuration | Executed scope | Result |
| --- | --- | --- |
| Static Release/O3/NDEBUG | Four model/context tests, five existing AT-SPI tests, native budget component and three new OS-client cases | 13 passed across preserved focused runs; no skips |
| Shared Release/O3/NDEBUG | Three model/context tests, five existing AT-SPI tests and three new OS-client cases | 11/11, no skips |
| Static Debug ASan/UBSan | Static scope above, halt-on-error and leak detection enabled | 13/13, no skips or sanitizer findings |

Static Release uses the unchanged production-source hashes from the initial
permanent run: its twelve passes remain preserved. The corrected/expanded final
host/client passed private Wayland separately, then X11 and session discovery.
This avoids relabeling the initial failed Wayland test as a pass. Shared and
sanitizer suites execute the final host/client in one run per configuration.

The independent C17 client links system AT-SPI/GObject/D-Bus, without NexaUI
linkage. It verifies complete 67-member sets, direct/selected ordinal asymmetry,
null selected references, offscreen selected states, table custom membership,
actual model acknowledgements, required/disabled policies, allocation failure and
retry, copied selection/state events and equivalent-operation idempotence.
Forty stale cases cover item/bulk before/after forwarding across ten transitions:
raw ancestor, member role, owner role, detach, context, model revision, model target,
disabled cycle, Multi-to-Single and dispatched disconnect/reconnect. The client
rediscovers the actual application through registryd after reconnect.

Twelve real allocator/deallocator cases exercise core/native detach, destroy and
nested publication, including bridge-allocation reentry. A core allocator callback
pumps actual native IPC while one request is in flight: 63 OS button actions are
accepted, the next is rejected, and exactly 63 clicks are delivered later.

The separate C component compiles the actual adapter once and invokes its real
D-Bus Selection method parser locally. It constructs 65,533 members plus three
other semantic nodes. Three recursive in-flight payloads consume
2,097,176 / 4,194,352 / 6,291,528 bytes; a fourth exceeds 8 MiB and is rejected before
core allocation, with every actual model unchanged and accounting returned to zero.
This is maximum-catalog native payload evidence, not external 65,533-member IPC
coverage. The external test's complete-set size is 67.

## Sensitive negative controls and preserved diagnostics

The identical final Shared Release host/client programs pass with the positive
library and fail with three deliberately different loaded libraries. `/proc`
records the actual host PID/birth, executable hash, mapped SDK path and hash:

- Previous 12ce9c5 provider: no MULTIPLE Selection interface; exact interface check
  fails before later cases.
- Native lease validation deliberately bypassed: an ancestor-stale request changes
  the actual model from zero to one selected member; unchanged-model check fails.
- Native slot accounting deliberately omits in-flight requests: the 64th OS button
  action is incorrectly accepted; exact rejection check fails.

A separate native C budget mutation omits in-flight bytes. It incorrectly accepts
the fourth payload and fails the precise byte-bound assertion. This component is
a separately linked program, not an identical-executable library control.

All initial failures remain retained. The expanded fixture initially included an
unused private header without its include path, then the component lacked the
test-hook macro needed to prevent a duplicate adapter from being pulled from the
archive; both were build/fixture corrections. The first Wayland slot assertion
read a per-frame counter after an extra frame reset it. Diagnostics prove routing
of the expected 63 clicks in the prior frame; a cumulative counter fixes the
oracle without changing production code. Disconnect testing initially required
an available transport during its one-second retry interval, then paused the
publication that performs retry. Final testing explicitly dispatches connection
loss before starting a frame and keeps transport publication retry alive while
model routing is paused. No deadline extension or failure suppression was used.

## Evidence and remaining gates

Actual C17 compiler recipes are retained for the adapter, Linux window wrapper,
host/client and native component: fourteen selected commands across the three
configurations. Release commands contain O3/NDEBUG; sanitizer commands contain
ASan/UBSan and frame pointers. Eight native programs and three SDK artifacts are
hashed. Link maps confirm that the component contains one adapter implementation;
the independent client's dynamic dependencies contain no NexaUI library. Two
separately installed static/shared C17 consumers compile, link and run complete
67-member model/semantic binding checks through the installed package.

The public header matches the base's exact filtered checkout bytes (the checkout
uses CRLF). ELF comparison removes no `nx_` symbol and adds only the two private
Linux wrapper helpers `nx_linux_window_ax_current` and
`nx_linux_window_ax_revision`; this is not a claim that ELF private visibility
was tightened. The four observed positive/negative host PID/birth pairs are
terminal, and no exact fixture executable remains live. Existing Clang warnings
in unchanged layout/headless/core sources are retained, not suppressed.
An additional build with `NEXAUI_ENABLE_ATSPI=OFF` compiles the actual no-SDK
adapter branch and runs the original optional native host successfully on a
private Xvfb display; ordinary window creation remains available.

Originals and machine-readable records remain in the task's evidence directory:
`out/atspi-multiselection-v1` (old baseline), `v3`/`v4` (original permanent failure
and diagnostics), `v5` (corrected slot oracle), `v7` through `v10` (reconnect fixture
development), `v11` (final static Wayland), `v12` (final matrix/build identity), and
`out/atspi-multiselection-controls` (negative providers and loaded-program identity).
Early iteration and build artifacts are retained separately.

No public header/API or Windows/macOS production source is changed by this
addition. Native Table/Text/EditableText/Value/Collection/relations, broad event
coverage, maximum-size external IPC, arbitrary module unloading, additional
architectures/desktops, and human/physical assistive acceptance remain outside
these local results. Application adoption and the NexaCloud pin are separate.

The permanent tests are `nexaui_atspi_multi_x11_private`,
`nexaui_atspi_multi_session_discovery_private`,
`nexaui_atspi_multi_wayland_private`, and static-only
`nexaui_atspi_multi_payload_budget`. The full Linux CI's executed-Wayland gate now
requires the new private Wayland case and rejects a skip.
