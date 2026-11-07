# Manifest source switching — 7 November 2026

Implementation/test source:
[`73eab048195dd8701336ad26971e88e8d6ec39c3`](https://github.com/Lulus792/NexaUI/commit/73eab048195dd8701336ad26971e88e8d6ec39c3).
Base `704bb147fc30795b7dcf529274209dd027fe31f2`; installed SDK source remains
`ac0fb3b3b3aa53b2a4de927ec52a06deb33d5c36`, version 0.1.23. Original Aetheri
source remains `1fb45c7c21f914a2893457132b609be50ef4280d`.
This advances scoped L06/I01/I07/A04/A06/A10/D07/P04/P05/Q02/Q04 in packet 6,
without changing SDK APIs/ABI, dependencies, Aetheri's live source or consumer pin.

## Source and view lifetimes

[Example 31](../../examples/31_aetheri_manifest/README.md) now accepts additional
caller-selected `--source MANIFEST EXPECTED_SHA256` pairs. It copies their paths
and digests into an immutable owner that outlives every worker. Previous/Next
reuse existing semantic buttons. The header displays the selected ordinal;
standard native read-only Text exposes the complete expected SHA-256, explicitly
as an expected identity rather than publisher trust or verified installation.
This ordered input selector is not a filesystem picker or source-discovery API.

Before waking a worker, the UI binds its accepted channel ticket to the selected
immutable source. Bind/take use the existing producer mutex; a worker then reads
its taken immutable source without consulting later UI selection. Two pending
bindings match the existing two-slot S02 channel. Core checks cover copied input
lifetime, out-of-order take, duplicates, capacity, invalid indexes and three
available inputs. No new wire format, app service or toolkit is introduced.

A successful source-selection frame retires the old native publication and
context, creates a new unique view epoch, clears artifact-local selection and
explicitly loads the new source. The next frame restores focus to an enabled
chooser button, using its enabled neighbour at an endpoint. An old tree intent
in that same frame cannot start work for the superseded view. Existing requests
retain their capacity and immutable source until completed; late results are
disposed without changing the current source's state. Returning to an earlier
artifact creates fresh native objects, preventing ordinal-ID reuse from
retargeting retained Text/Action references. F4/F5 retain the selected source.

The original verifier and original canonical vectors/encoder remain the actual
data source. Reads still open only caller-selected manifest files. There is no
payload access, execution, inventory discovery, grant, publisher authentication,
catalogue mutation or install/remove request.

## Executed evidence

Same Fedora 44/GCC 16.2.1/Wayland 1.26.0 private guest, frozen original compositor,
licensed font and installed SDK artifacts as the [preceding report](manifest-hierarchy-report.md).
The SDK/consumer builds remain strict C17, global `-Werror`, Wayland-only software
with AT-SPI. ASan/UBSan instruments the consumer and original Aetheri core; leak
detection remains enabled. These are newly executed external consumer checks,
not a new local SDK-wide suite or a performance/physical acceptance claim.

| Profile | Manifest CTests | Neutral CTest | Native source workflow | Native regressions |
| --- | ---: | ---: | --- | --- |
| Static Release | 4/4, no skips | 1/1, no skips | 9/9 held-worker steps and 9/9 ungated light-theme steps | 11/11 single-manifest, 18/18 neutral |
| Shared Release | 4/4, no skips | 1/1, no skips | 9/9 held-worker steps | 11/11 single-manifest, 18/18 neutral |
| Static Debug ASan/UBSan/LSan | 4/4, no skips | 1/1, no skips | 9/9 German 200% at 360×260/high contrast | 11/11 single-manifest and 18/18 neutral, both German 200%/small/high contrast |

The independent libatspi client links neither NexaUI nor Aetheri. It checks
complete expected SHA-256 Text, chooser enabled/focus/bounds, all original path
names, selection, over-height keyboard reading, retained references and actual
A→B→A→B source changes. Shift-Tab/Enter changes the source while A's result is
held; native button actions perform the reverse and subsequent changes. B's
newer result publishes first; A's late result cannot alter B's selection or
revision. Old Buttons, branch/row objects and held Text interfaces refuse reuse,
including on return to the exact earlier artifact. A disabled Next cannot submit
another request. Header glyphs/ordinals and both sources were visually inspected
in the captured 200% high-contrast case; complete human/design review remains open.

Each controlled source workflow closes with
`submitted=5 posted=5 adopted=3 ignored=1 drained=1 joined=2 failed=0`.
Its last held, already decoded result posts after `GUI_GONE`, then both workers
join before source/channel/font destruction. The ungated workflow closes with
`submitted=5 posted=5 adopted=5 ignored=0 drained=0 joined=2 failed=0`.
All accepted hosts/peers exit 0 without sanitizer diagnostics. Arbitrary pending
filesystem I/O latency is not bounded by a post-decode hold test.

Two private counterexamples reproduce the missing guarantees. Binding every
ticket to source A fails the ownership core test and native stage 2, where the
selected B view otherwise presents A's paths. Reusing the old context/provider
fails native stage 1 on retained branch/button/Text references. Original failures
are retained: the new peer first expected the wrong native role for read-only
Text, then queried its retired application proxy after source replacement.
It now checks the actual label role/Text interface and rediscovers the live
application, while keeping old-reference rejection and all original deadlines.

## Reproducible artifact and remaining work

Bundle: `D:/NexaUI/out/nexaui-source-switch-20261107.tar.gz`.
SHA-256: `899ad9298abc6b573dc6ce618a4aad2c0711d79f6b5c559dfee91e98ab85fa07`.
29,313,252 bytes; 2,406 regular files and 23 symbolic links plus the manifest.
Archive SHA, every listed file/size/hash and symbolic link were independently
verified after local transfer. It retains source/build commands, exact artifacts,
raw captures, all positive/negative cases and initial failures. Source proof
checks 1,054 committed files, compares inputs already present during compilation
before materializing remaining archive files, verifies 20 original Aetheri
inputs and 159 unchanged SDK production/build/data inputs. Only the historical
root CTest registration differs from the reused SDK source. Newly copied owned
source mtimes were normalized; no clock or global environment changes were made.
After verification, 30 loose capture duplicates (421,632,510 bytes) were retired
only from this step's explicitly listed owned cases. Raw bytes remain in both
verified bundles. The record is
`D:/NexaUI/out/source-switch-capture-retirement-20261107.json`; restore those PPMs
from the archive before rerunning its collector. No earlier frozen bundle changed.

This closes the previous sample's single-artifact switching restriction for
caller-supplied sources. Friendly source names/search, dynamic source-set updates,
per-source UI-state restoration, generic multi-branch/paged trees, inventories,
actual services/authorities, full software/device/file pickers, trusted system
surfaces, normal Aetheri adoption and physical/human/performance gates remain
open. Native cases use two sources; the core also verifies three available
inputs. Original Files still scans synchronously. Existing paused acceptance
tests remain paused; the full 103-ID catalogue is not complete.
