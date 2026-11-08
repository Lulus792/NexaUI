# Software native output, scale and workarea transitions — 2026-11-08

Consumer/test source: `665f930a5ec5bdbcd6167411f2a5e44a78ce9e7e`.
Installed SDK: `c4f467d85b0147944d2456f5e7602a39ac18e579`, NexaUI0.1.23.
Original catalogue: `46e2a21ef979d9a76db2a8487d1de167a6a79ef7`,73 checked inputs.
Frozen compositor producer: `d56d69523c33a48648af343e7d36526f84518c90`,17 checked inputs.
Catalogue SHA-256: `9c932872caeba8007bf05b86f7c167b493260709b23c54d709222a5f19539e65`.
Scoped W03/W04/W05/D07/L05/L11/I01/I05/T11/A02/A06/P05/Q02/Q03/Q04/Q08 evidence, packet6.

The optional [Software consumer](../../examples/34_aetheri_software/README.md)
now has a reproducible native output-transition task. Existing public window,
software presenter, context raster-scale, input, worker and accessibility APIs
already support this controlled scope. The ordinary host adds pixel-size/raster
diagnostics; no SDK implementation/API/ABI/version, product view/model/worker,
active Aetheri source or reviewed dependency pin changes.

## Producer and behavior

The separate [output producer](../../examples/34_aetheri_software/output_producer/CMakeLists.txt)
reuses the source-checked Files IME producer and its pinned system dependencies.
It generates copies of main/input-driver files from the immutable export; each
exact replacement is recorded and replayed by the host verifier. The inherited
private socket adds bounded compositor-owned output placement, scale, bottom
reservation and maximize/restore commands. These commands are test policy, not
an installed desktop/privileged API or proof of a client-initiated native drag.

The frozen original has viewporter but no fractional-scale manager. The private
copy advertises that manager, sends actual preferred-scale events and computes
configure sizes in logical output coordinates. Native pointer injection uses
the current output-layout bounds after scale/mode changes. No change is made to
the original Aetheri producer or its independent development checkout.

Each output task preserves a Unicode draft and caret5 through eleven transitions:

1. Output0 changes from1x to1.25x.
2. The compositor places the same window on output1 at1.5x, then output2 at2x.
3. Maximize gives1920x1080 logical/3840x2160 pixels on output2.
4. A96-logical-pixel bottom reservation changes the client to1920x984.
5. Output2 changes mode to3200x1800 pixels; its2x client becomes1600x804.
6. Restore returns the original normal size; subsequent placement/scale changes
   visit output1 and output0, then restore1x and the original field geometry.

The initial physical output modes are1920x1080,2560x1440 and3840x2160.
All outputs remain enabled; gaps between their fixed logical positions are
allowed. There is no real dock in this standalone fixture: the reservation is
explicit compositor policy. Hotplug/removal, a normal desktop owner and physical
monitor movement are not covered by this task.

For every transition the parent requires a newly ready frame, exact public
logical/pixel dimensions, matching context raster scale and the compositor's
actual committed surface buffer/logical dimensions. Owner state, epoch, match
count, mode, request sequence and focus must remain unchanged. The independent
libatspi peer checks the same retained field is live, focused and contains the
complete draft/caret; field height remains logical and action bounds stay above
it. Final real pointer/key input appends text at the returned field's end.
Cancel retires the old field and rejects its later edit. Local inspection and
all output changes leave backend requests at2 and original database bytes/rows
unchanged. Hosts and peers close normally.

The archived Wayland traces independently contain the exact main-surface
enter/leave sequence0→1→2→1→0 and preferred-scale sequence120/150/180/240/180/
120/150/120 in all seven output/capture cases. Cursor-surface events are kept
separate. The host verifier checks these actual wire events rather than inferring
output migration solely from the test command or preferred density.

## Executed checks and artifacts

| Native tasks | Static Release | Shared Release | Debug ASan/UBSan/LSan |
| --- | ---: | ---: | ---: |
| EN16px and DE32px/high-contrast output transitions | 2/2 | 2/2 | 2/2 |
| DE compact resize/Escape/draft-focus regression | 1/1 | 1/1 | 1/1 |
| DE native preedit/commit/navigation regression | 1/1 | 1/1 | 1/1 |
| Four-stage three-output capture repeat | 1/1 | Not repeated | Not repeated |

There are13 positive cases,258 independent checkpoints and77 checked output
transitions. Each consumer profile compiles28 actual C17/Werror objects; the
private producer compiles six. The independent OS peer is also compiled as
C17/Werror. No core SDK or component CTest rerun is claimed for these host/test
changes; the previous [caption/component checks](software-label-report.md) remain
at their original source. That published source now has [seven successful
required CI jobs and two optional skips](software-label-ci.json).

The existing unprivileged private Linux x86-64 VM, software presenter and private
AT-SPI bus/registry are reused. Release/shared SDK prefixes and sanitized SDK/
consumer/original-catalogue builds retain their exact hashes. The compositor,
independent OS peer and OS libraries are not fully sanitizer-instrumented; no
TSan, GPU, physical-display, human-screenreader or performance claim follows.
Carrier/toolchain provenance remains in the [lifecycle record](software-lifecycle-report.md).

Twelve complete original output captures are retained. Four full RGB buffers
were converted losslessly to PNG and inspected: [1.25x](images/software-output-scale125.png),
[2x](images/software-output-scale200.png), [reserved workarea](images/software-output-workarea.png)
and [returned1x](images/software-output-returned.png). They show complete German
captions, the retained Unicode draft/caret and bounded reflow. The help text uses
its existing clipped scroll viewport. This is the opaque high-contrast fixture,
not final visual or physical text-sharpness acceptance. [Image bindings](software-output-images.json).

Two independently compiled native defects fail these checks: fixed1x raster
density times out at the first1.25x presentation boundary; doubled logical layout
passes pixel/raster checks but the OS peer rejects its field bounds. Sources,
objects, binaries and original failure logs are preserved. Preliminary producer
include/peer-tooling failures, test-focus sequencing, a cleanup GLib warning and
the collector's manifest-shape error also remain archived and separately
attributed in [checks](software-output-checks.json). The warned earlier run is
excluded from the final matrix; all13 final cases have clean checked logs.
No old deadline, assertion or sanitizer was weakened.

The earlier750c7dc native ASan Read-action failure remains an **open AT-SPI
stability finding**. Its unchanged retry and these new positive cases establish
no cause or repair. See the [original finding](software-label-checks.json).

## Binding, adoption and remaining scope

Published checkpoint `e0c7a7f4bda3e23cdbe5e07e07dbdf86e89db1de` passes all seven
required CI jobs in [run42505097233](https://github.com/Lulus792/NexaUI/actions/runs/42505097233);
two optional jobs are skipped. The [job receipt](software-output-ci.json) is tied
to that source and does not replace the separate external-consumer evidence.

[Source binding](software-output-source-binding.json) matches102 executed inputs
to exact Git blobs after CRLF-to-LF normalization only. The README-only later
build instructions/report link are explicit. Four generated source transforms
are replayed. [Local results](software-output-local.json) and
[host checks](software-output-checks.json) bind382 artifact rows,73 original
catalogue inputs and17 producer inputs to preserved current/support archives.

The fully verified host archive is
`D:/NexaUI/out/nexaui-software-output-20261108.tar.gz`,16,175,593 bytes, SHA-256
`f8f1b2382d0f4be3bba06a377f8c153d6e5ccea4449675730e491c21bfe7451f`.
[Archive receipts](software-output-archive.json) also identify reused frozen SDK/
original/runtime evidence. No task artifacts were retired for this checkpoint.

Aetheri can evaluate this selected-app presentation independently using existing
public contracts; no pin/default-owner change is requested. The private output
producer is a regression tool, not a replacement compositor. Output removal/
hotplug/resume, broader fonts/locales/IME/clipboard, full inventory and device/
process/trusted-service workflows, normal adoption and final hardware/human/
performance/design gates remain. Remaining packet6, packets7–8 and all103 IDs
stay open at their actual scope; no whole requirement or1.0 is declared here.
