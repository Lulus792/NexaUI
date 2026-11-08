# Software native held-query lifetimes — 2026-11-08

Test/consumer source: `7c45be1f166ade719bdc78688eca75840ac8c1e9`.
Published checkpoint `49becec512f47f21a5e3e0e34d17404f8007b3d5` has
[seven successful required CI jobs and two optional skips](software-lifecycle-ci.json).
The later [ordinary clipboard/resize tasks](software-window-report.md) retain
their separate source and native evidence; the historical scope below is unchanged.
Unchanged ordinary host/model/view/worker: `8b29872a2bccec620f453cc2a97e7ef796ecee64`.
Original Aetheri: `46e2a21ef979d9a76db2a8487d1de167a6a79ef7`.
Installed SDK: `c4f467d85b0147944d2456f5e7602a39ac18e579`, NexaUI0.1.23.
Catalogue SHA-256: `9c932872caeba8007bf05b86f7c167b493260709b23c54d709222a5f19539e65`.
Scoped D07/L05/L11/I01/I05/A02/A06/P04/P05/P07/Q02/Q04/Q08 evidence, packet6.

The [selected-app consumer](../../examples/34_aetheri_software/README.md) adds
a default-OFF lifecycle host and an independent OS libatspi peer. The host
includes ordinary `native.c` verbatim; linker wrappers observe original queries,
native waiting and retirement. No product host/model/worker, SDK implementation,
public API, ABI or version changed. Aetheri's active checkout/pin is independent.

## Actual execution

| Checks | Static Release | Shared Release | Debug ASan/UBSan/LSan |
| --- | ---: | ---: | ---: |
| Ordinary worker/view CTests | 2/2 | 2/2 | 2/2 |
| English + German large/high-contrast held wake/close | 4/4 | 4/4 | 4/4 |
| Ordinary German large/small-window controlled IME/navigation | 1/1 | 1/1 | 1/1 |

All15 native cases pass, with66 checkpoints:12 held cases ×3 and three ordinary
regressions ×10. Normal hosts and peers exit0 without forced termination.
Original fixture Root/User database bytes and rows remain unchanged in each case.
Native host maps reject SDL/X11/EGL/Vulkan presentation dependencies; the existing
software window/presenter path is used. There are no new SDK core test runs.

The carrier is the existing unprivileged private Fedora Linux x86-64 VM, GCC16.2.1,
CMake4.3.0 and original headless Aetheri compositor. Release uses CMake Release;
the selected Debug profile instruments the installed static SDK, original
catalogue/provider and native test host with address/undefined sanitizers and
leak detection. OS libraries and the independent OS peer are not all instrumented.
This is no TSan/general race, physical input, human assistive or performance proof.

## Query, wait, retirement and allocation boundaries

The second complete `sw_read` is held at the first real original
`ae_catalogue_resolve_app` SELECT, after SQLite BEGIN. `sqlite3_get_autocommit`
must confirm an active transaction. Original open/read/close remain serialized
on the worker. Both scans validate five actual selected-target results: two
eligible active-app observations and three original `-ENOENT` outcomes. Those
unknown outcomes do not mean absent/uninstalled, nor supply a complete inventory.

While held, the OS peer observes Reading, disabled Read and enabled Close.
In wake cases the parent reads its actual child's syscall and bounded pollfd
array to witness the native main thread waiting with the worker FD. Releasing
SQL must yield a readable-FD return, accepted epoch2 and unchanged native key
count before further input. The OS peer then refuses the old row's action,
finds the replacement with the same complete installation ID and observes Read
enabled again. This is a completion/wakeup check, not a CPU/energy budget.

In close cases the peer opens the real filter, retains an editable draft and
focuses Close. Native Enter retires/unmaps the GUI while SQL remains held and
the worker remains alive. Retained AT-SPI field/button references must reject
edits/actions before the parent releases the query. The late result is not
adopted: final counters are requests2/adopted1/ignored0/failed0, versus adopted2
for wake. Both actual result allocations (rows and result) are freed exactly
once on the GUI thread after view/window retirement; original catalogue close
remains on its worker. Entry/exit descriptor counts balance after accounting
for the deliberately closed control-gate FD.

`sw_snapshot_release` and `sw_result_free` share a translation unit. The test
therefore wraps their actual external allocator releases, rather than assuming
the linker can interpose an intra-object call. Copied worker scope/targets and
reply/channel ownership retain their existing contracts. No query timeout or
cancelable filesystem shutdown is claimed: GUI retirement precedes an eventual
worker join, which still needs the original query to return.

## Negative controls, source and artifacts

Two independently compiled worker defects are caught by unchanged native checks:
omitting only the second completion's pipe write leaves the main thread asleep
and fails the existing completion deadline after the original read returns;
disposing a late result on the worker after stop fails the GUI-thread assertion.
These expected failures have separate source/object/executable/logs; their
cleanup is not presented as successful ordinary shutdown.

[Source binding](software-lifecycle-source-binding.json) compares the staged
inputs to exact Git blobs after CRLF→LF only. The README was expanded after
execution; that documentation-only difference is explicit. [Local evidence](software-lifecycle-local.json)
and [host verification](software-lifecycle-checks.json) distinguish configured
compile commands from14 actual compiled consumer C17/Werror objects and17
original catalogue/core objects per profile, and bind232 artifacts,
73 original inputs, all15 native results and the two controls. Three actual
linked SDK probes return0.1.23, exactc4f467d and features116. A separate default
configuration confirms the held host is absent when its option remains OFF.

The fully verified final host archive is
`D:/NexaUI/out/nexaui-software-lifecycle-20261108.tar.gz`,24,148,295 bytes,
SHA-256 `85425fff953e7a34f8a07064e4d53b33322f8239b97cfe0837669dd379a10f70`.
It retains source, binaries/objects/libraries, installed SDK prefixes, copied
external original/runtime inputs, commands and logs. The separate fully checked
early archive preserves earlier attempts and retired Release/shared view-test
captures:10,980,733 bytes, SHA-256
`c6597e4cf2dcc5cca3425e24036da1ab73fa81b7d7dbcd33326b6643548017d2`.
See the [archive receipts](software-lifecycle-archive.json).

Retained unsuccessful preparation: the first export omitted existing common
text-wrap sources and failed configure; a subsequent view run hit the private
user's storage quota, then a sanitizer link hit ENOSPC. Only fully archived,
hash/size/mode/path/UID/process-checked prior Files build trees and completed
current view-test outputs were retired. Successful worker/view checks then ran
with original assertions/deadlines/instrumentation. The first native attempt
failed before app start because its private bus socket path was too long;
shorter unique test directories fixed that environment issue. No global quota,
cache, system configuration or Aetheri source was changed.

## Remaining scope

The next Software checks are foreign clipboard/late replies and native resize/
output transitions. Complete app/tool/component discovery, management plans and
authorized service effects, real device/process metrics, trusted login/lock and
installer integration remain explicit owner seams. Broader locale/font/IME,
driver/hardware/human/performance/adoption acceptance, remaining packet6 and
packets7–8 stay open across all103 IDs. This checkpoint closes no full catalogue
row and declares no1.0 release.
