# Software output removal and fresh recreation identities — 2026-11-08

Consumer/test source: `8c3537dc9b1dbe7cd605d63ec8d393d3ef208ff1`.
Installed SDK: `c4f467d85b0147944d2456f5e7602a39ac18e579`, NexaUI0.1.23.
Original catalogue: `46e2a21ef979d9a76db2a8487d1de167a6a79ef7`,73 checked inputs.
Frozen compositor: `d56d69523c33a48648af343e7d36526f84518c90`,17 checked inputs.
Catalogue SHA-256: `9c932872caeba8007bf05b86f7c167b493260709b23c54d709222a5f19539e65`.
Scoped W03/W04/W05/W10/L11/I01/I05/A02/A06/P05/Q02/Q03/Q04/Q08 evidence, packet6.

The optional [Software consumer](../../examples/34_aetheri_software/README.md)
now has real private output destruction/recreation checks in addition to the
[preceding output transitions](software-output-report.md). Existing public
window/output-snapshot/input/raster/worker/semantic APIs support this bounded
scope. No SDK implementation/API/ABI/version, ordinary `native.c`, product view/
model/worker, active Aetheri source or reviewed pin changed.

## Ownership and observed behavior

The separate source-checked output producer adds two inherited-socket commands.
`R index` relocates the sole mapped window to a remaining output if necessary,
then really destroys the selected headless output. It refuses removal of the
last output and never modifies a DRM/nested session. `N index scale120` creates
a new headless output in the empty slot, with fresh Wayland advertisement,
render state, mode, scale, scene and layout. This is test compositor policy,
not an installed desktop interface or proof of physical connector behavior.

The extended task retains the previous11 scale/workarea transitions, then:

1. Selects1.25x and maximizes on output0 at1536x864 logical size.
2. Removes output0. The same maximized window becomes2560x1440 on output1 at1x.
3. Recreates output0 at1.25x while the window stays on output1; restores normal
   size and moves the same window onto the new output0.
4. Removes unused output1, recreates it at1.5x, moves onto it, removes it while
   current, and recreates it again at1x. The window stays usable on output0.
5. Returns output0 to1x, edits the retained field through actual pointer/key
   input, cancels it and refuses a later edit through the retired OS object.

All three output-recreation cycles require complete public snapshots with
increasing revisions and counts3→2→3→2→3→2→3. Each removed registry ID disappears;
every recreated ID must be new across the whole case. Connection-local IDs are
never persistent device identities or authority. Native window/model request
state, Unicode draft, caret5 and logical field height remain unchanged. The
independent libatspi peer checks the live field, focus, full text and header/
field geometry at every phase. Backend requests stay at2; original Root/User
database bytes and rows remain unchanged.

`SWG_BUILD_OUTPUT_HOST=ON` adds the default-off `software_output_native` observer.
It includes the ordinary native host verbatim and wraps public get-info/poll
calls to record output snapshots and wait boundaries. The16-record bound is
private to this three-output fixture and fails explicitly if exceeded. The
ordinary host retains its original behavior and no second UI loop/thread/window
is introduced. The installed SDK, not the observer, owns output subscription,
coalesced wakeups, snapshot readiness, native routing and rendering.

Before every topology command the parent observes its actual Linux x86-64
child in `poll`, reads its bounded pollfd array and verifies the worker FD.
This yields42 real sleep witnesses, with no added keyboard input across these
changes. It is a bounded waiting/lifetime check, not an idle-power budget or
proof that only one kind of native event can wake the host.

## Executed evidence

| Native tasks | Static Release | Shared Release | Debug ASan/UBSan/LSan |
| --- | ---: | ---: | ---: |
| EN16px and DE32px/high-contrast hotplug tasks | 2/2 | 2/2 | 2/2 |
| Ordinary-host preceding output transitions | 1/1 | 1/1 | 1/1 |
| Ordinary-host compact resize/Escape regression | 1/1 | 1/1 | 1/1 |
| Ordinary-host IME/navigation regression | 1/1 | 1/1 | 1/1 |
| Six-stage three-output capture repeat | 1/1 | Not repeated | Not repeated |

The16 positive cases retain414 independent checkpoints,194 checked transitions,
42 topology snapshot boundaries and42 sleep witnesses. Each consumer profile
compiles29 actual C17/Werror objects; the private producer compiles six. The
independent OS peer is also C17/Werror. The first completed Release case is
reused only after its exact runtime input hashes and result are checked; no
failed run is silently retried. All final checked logs are clean; hosts/peers
exit0 and cleanup needs no SIGKILL.

Two separately compiled wrong observers fail: one freezes the first snapshot
and is rejected at removal; the other substitutes old IDs by physical-mode
width and is rejected at the first recreation. Their sources/objects/binaries
and precise failure logs remain separate from positive cases. These are coverage
controls, not claims of a repaired SDK defect. No deadline/assertion/sanitizer
was weakened. No core/component CTest rerun is claimed for these optional host/
test changes; previous checks retain their original source attribution.

Archived Wayland traces independently bind public registry IDs to actual
output objects, all three `global_remove` events and the same main surface's
17-event enter/leave sequence through the removed/recreated outputs. Extra
preferred-scale notifications during recreation are retained; the parent
requires correct settled logical/pixel/context-raster dimensions and actual
compositor-consumed buffers at every boundary. Three ordinary-host regression
traces retain their preceding exact scale sequence. [Wire and artifact checks](software-hotplug-checks.json).

All18 full output captures remain in the archive. Two complete RGB buffers were
losslessly converted and inspected: [maximized failover on output1](images/software-hotplug-failover-maximized.png)
and [normal-size return on recreated output0](images/software-hotplug-returned.png).
They show complete German captions and the retained draft/focus; the returned
caret is visible and the help retains its clipped scroll viewport.
[Image bindings/review](software-hotplug-images.json). Opaque high contrast is
the tested fallback, not final visual or physical sharpness acceptance.

The unprivileged private Linux VM and frozen SDK/original/font/registry inputs
are reused from the [lifecycle record](software-lifecycle-report.md). SDK,
consumer and original-catalogue sanitizer profiles are instrumented; compositor,
OS libraries and independent OS peer are not fully instrumented. No TSan,
hardware/GPU/session suspend, human screenreader or performance proof follows.
The earlier750c7dc ASan native Read-action failure remains an open AT-SPI
stability finding; these successes establish no cause or repair.

## Source binding and remaining work

[Source binding](software-hotplug-source-binding.json) matches103 executed
inputs to exact Git blobs after CRLF-to-LF only. The README-only later task
instructions/link are explicit. Four generated source transformations replay
exactly. [Local results](software-hotplug-local.json) and
[host checks](software-hotplug-checks.json) bind412 artifact rows,73 original
catalogue inputs and17 producer inputs to verified current/support archives.

Full host archive: `D:/NexaUI/out/nexaui-software-hotplug-20261108.tar.gz`,
12,552,495 bytes, SHA-256
`13bb6156d61b30629c69ae20ece0391c87b4b8b77e3d7395b7bfb185440e6616`.
[Archive receipts](software-hotplug-archive.json) include the reused frozen
SDK/original/runtime support. No artifacts were retired for this checkpoint.

Aetheri can evaluate the existing selected-app presentation independently; this
does not request a pin/default-owner change or supply production output policy.
All-output loss, long-running repeated hotplug/resource budgets, physical
connectors, GPU/session suspend and normal-owner adoption remain open. Full
inventory/device/process/trusted-service compositions, broader locale/input,
final design/human/performance gates, remaining packet6 and packets7–8 persist
across all103 requirements. No complete catalogue row or1.0 is declared.
The next reusable SDK gap to reconcile is W01's cancellable native-close
contract; unavailable physical tests must not block that independent work.
