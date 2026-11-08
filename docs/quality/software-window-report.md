# Software foreign clipboard and native compact resize — 2026-11-08

Test/consumer source: `ad7c933fe2763a4893758d31c3558ac56ad6bf31`.
Published checkpoint `70408963bd6f52ea43cb07a82c28f06cb79bcee1` has
[seven successful required CI jobs and two optional skips](software-window-ci.json).
The later [measured-caption/compact-focus increment](software-label-report.md)
addresses the observed label wrapping with its own source and evidence boundary.
Unchanged ordinary host/model/view/worker: `8b29872a2bccec620f453cc2a97e7ef796ecee64`.
Installed SDK: `c4f467d85b0147944d2456f5e7602a39ac18e579`, NexaUI0.1.23.
Original catalogue: `46e2a21ef979d9a76db2a8487d1de167a6a79ef7`,73 checked inputs.
Original private compositor producer: `d56d69523c33a48648af343e7d36526f84518c90`,
17 checked original inputs and the same two generated IME test inputs used by Files.
Catalogue SHA-256: `9c932872caeba8007bf05b86f7c167b493260709b23c54d709222a5f19539e65`.
Scoped W03/D07/L05/L11/I01/I05/T11/A02/A06/P05/P07/Q02/Q04/Q08 evidence, packet6.

The [ordinary selected-app consumer](../../examples/34_aetheri_software/README.md)
already binds native clipboard and IME. Its existing OS peer/driver now exercise
foreign clipboard and real pointer resize, reusing the Recovery fixture's
transport/capture/late-send witnesses. No product host/model/view/worker, SDK
implementation, public API/ABI/version or independently developed Aetheri pin
was changed. These are selected-target observations, not complete inventory.

## Executed matrix

| Native tasks | Static Release | Shared Release | Debug ASan/UBSan/LSan |
| --- | ---: | ---: | ---: |
| English + German large/high-contrast foreign copy/paste + late focus reply | 2/2 | 2/2 | 2/2 |
| Same two styles, close while foreign source remains stopped | 2/2 | 2/2 | 2/2 |
| Same two styles, pointer64×64 →240×96 → original-size resize | 2/2 | 2/2 | 2/2 |
| German large/small-window native capture repeat | 1/1 | Not repeated | Not repeated |
| Ordinary controlled IME/navigation regression | 1/1 | 1/1 | 1/1 |
| New driver with prior held-query wake/close host/OS peer | 2/2 | Not repeated | Not repeated |

All24 native cases pass,327 checkpoints:18 initial tasks with276 checkpoints,
one15-checkpoint capture repeat and five regressions with36 checkpoints. Normal
hosts/peers exit0; no final positive cleanup child requires forced termination.
The original fixture Root/User database bytes and rows remain unchanged. Native
maps exclude SDL/X11/EGL/Vulkan presentation dependencies. These tests use the
existing software presenter and original headless compositor at1×; unchanged
backgrounds on three outputs do not prove output migration, fractional scale or
physical multi-monitor acceptance.

The carrier/compilers/installed SDK are those of the [prior lifecycle checkpoint](software-lifecycle-report.md).
The independent peer compiles as C17 with warnings as errors. Actual SDK,
ordinary host/model binaries and installed prefix bytes are matched to the prior
fully verified archive. No new SDK/core or ordinary component CTest run is
claimed; previous two-CTest/profile results retain their original attribution.
The selected sanitizer profile instruments the existing SDK/provider/original
catalogue/native host; OS libraries and the independent OS peer are not all
instrumented. There is no TSan/general race or performance-budget proof.

## Actual clipboard and retirement boundaries

Independent wl-clipboard2.2.1 programs run through their private musl loader;
all five tool/runtime files match preserved Files evidence. A foreign producer
publishes `Grüße 🌍 é` (including a combining mark). The test restores actual
pointer focus, pastes through native Ctrl+V and checks exact text with OS AT-SPI.
Native Ctrl+A/C then replaces the foreign owner; an independent reader must
receive the same complete UTF-8 bytes. Publication, source cancellation and
complete native key consumption are observed rather than inferred from a sleep.

For the delayed reply, a second real foreign owner publishes another value and
is stopped. The driver observes its stopped process and the host's actual native
receive request. In normal cases, native Tab first leaves the filter. Only then
is the owner resumed; an actual parent-owned syscall/socket witness establishes
that its send callback returned before a fresh native frame/OS text check.
The late value must not replace the retained Unicode draft. Escape retires the
field and restores Filter focus and the selected full-ID row.

In close cases, Close is focused beside the pending field. Native Enter unmaps
and exits the host successfully while the foreign owner remains stopped. The OS
peer must reject edits/actions on retained field/button references before final
cleanup resumes the source. No GUI wait for that producer is needed. These are
small successful/late/canceled transfers; payload-budget pressure, deadlines,
owner loss, assistive Cut/Paste and comprehensive clipboard error feedback remain
separate acceptance. Visible emoji fallback is not proven by UTF-8 transport.

## Resize, images and known visual work

Resize uses the actual bottom-right pointer grip and compositor grab/configure/
release sequence. At64×64 and240×96 the old filter reference is retired, its late
edit is refused and the independently measured Close bounds remain inside the
client. Returning to the original size restores the Unicode draft and field
focus; Escape retires that replacement and restores Filter/full-ID selection.
State, epoch, filtered count, mode and backend request number remain stable
through resize. This does not add a text-selection/history preservation claim.

All nine final PPM files are complete, hash-bound native captures across the
unchanged1920×1080,2560×1440 and3840×2160 outputs. Three full output0 images were
losslessly converted and inspected: [filter](images/software-window-filter.png),
[compact](images/software-window-compact.png), [restored](images/software-window-restored.png).
The captured filter/restored full RGB buffers are identical; compact differs.
The64×64 Close remains visible/focused and the draft/caret return. Large German
action captions still wrap inside words, and the narrow explanation viewport
needs its existing scrolling. Those remain visual/layout review work, not a
complete design acceptance. [Image binding and review](software-window-images.json).

## Controls, artifacts and retained preparation failures

Two independently compiled view defects fail the unchanged OS checks: removing
only native clipboard binding prevents the expected Unicode paste; clearing the
draft on compact restoration fails the restored text check. Their individual
source/object/binary/logs remain separate from positive hosts. Initial control
collection used the wrong diagnostic marker, then the draft mutant initially
used the wrong setter signature. Both preparation errors are retained; corrected
controls compile with the original warning policy and fail on the intended text
assertions. No deadline, assertion or sanitizer was weakened.

[Source binding](software-window-source-binding.json) checks95 staged inputs
against exact Git blobs after CRLF→LF only; the README-only post-execution update
is explicit. [Local evidence](software-window-local.json) and [host verification](software-window-checks.json)
bind266 artifacts against the new and prior archives,73 original catalogue inputs,
17 producer inputs, the unchanged producer binary/generated sources and five
foreign runtime files. The original compositor executable retains SHA-256
`1ab4ece87ae5f2436087baeb92e41f8dcdfcf9157a7f63712553699550dbf677`.

The full checked archive is `D:/NexaUI/out/nexaui-software-window-20261108.tar.gz`,
4,547,340 bytes, SHA-256 `6f15b7dce3e259d41494b1167551fde150cc5903d6d880f1e131ff3705a1768a`.
It retains the new tests, native tasks/captures, controls and private runtime/producer
inputs. Reused SDK/host/original inputs remain byte-bound to the24,148,295-byte
prior lifecycle archive. A3,591,328-byte early archive, SHA-256
`a6a94c54f6e7e15d7e2f91eb966b794684b4c36b873bba99e12cca1e5722cda7`, preserves
the incomplete first capture and failed preparation. [Receipts](software-window-archive.json).

The first capture's final output file was truncated, the compositor reported
capture failure and the native host observed disconnect; the subsequent first
regression compositor died with SIGBUS during startup. After verified backup,
exact inactive source/build/view/capture paths were retired using byte/hash/mode/
path/UID/process checks. The unchanged final capture and regressions then pass.
Storage pressure is supported by that intervention and the preceding quota
failures; these generic compositor errors alone do not identify a kernel cause.
No global quota/cache/system setting or live Aetheri source was changed.

## Remaining scope

Next close native output/scale/workarea transitions and the observed large-label
layout weakness. Complete app/tool/component discovery and management services,
device/process state and trusted login/lock/installer adapters remain owner seams.
Broader locale/font/IME/clipboard, hardware/human/performance/adoption acceptance,
remaining packet6 and packets7–8 stay open across all103 IDs. No full catalogue
row or1.0 release is declared from these scoped native tasks.
