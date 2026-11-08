# Root11 held-query wakeup and close lifetimes — 2026-11-08

Test source: `0d418830ac90270135ea26d9b1f8f0c317bd05ee`.
Unchanged provider/model/view/worker source: `e3031e0f382c67e41929d7c08e710ea7be7e4a7f`.
Explicit Root11 original source: `fa5347264054b05e8af57e6d0a210008a6c849d1`.
Legacy original source: `46e2a21ef979d9a76db2a8487d1de167a6a79ef7`.
Installed SDK: `c4f467d85b0147944d2456f5e7602a39ac18e579`, NexaUI0.1.23.
Catalogue SHA-256: `9c932872caeba8007bf05b86f7c167b493260709b23c54d709222a5f19539e65`.
Scoped D07/L05/L11/I01/I05/A02/A06/P04/P05/P07/Q02/Q04/Q08, packet6.

The [ordinary native Root11 profile](recovery-root-v11-native-report.md) now
also has original held-query completion/close evidence. The existing test-only
host still compiles the ordinary `native.c` entry unchanged and wraps only
the original query, SQLite transaction boundary, native wait and retirement.
The shared model/view/worker and installed SDK API/ABI/version are unchanged.

## Executed matrix

| Cases | Static Release | Shared Release | ASan/UBSan/LSan |
| --- | ---: | ---: | ---: |
| Root11 fresh/lost/replacement × wake/close | 6/6 | 6/6 | 6/6 |
| Legacy Root/User × wake/close | 4/4 | Not repeated | 4/4 |
| Ordinary Root11 IME/navigation regression | 1/1 | Not repeated | 1/1 |

All28 native cases pass:26 held cases with three checkpoints each and two
ordinary regressions with25 each, totaling128 checkpoints. Thirteen cases
prove wakeup; thirteen prove close before gate release. These are scoped
fixture results, not completed catalogue IDs or actual service authority.

For the new profile, both actual original inventory results must be Root11,
contain exactly one issuer kind17 with the full expected ID and related
operation, match the independently read catalogue runtime, and retain the
original `issuer_retirement_unsettled` reason. The independent OS peer retains
the specific issuer row rather than an unrelated first row.

## Transaction, UI and ownership order

The second original read is stopped inside its real SQLite read transaction
before the inventory query executes. The peer observes Reading, disabled Read
and enabled Close. In wake cases, a parent-owned Linux x86-64 syscall/pollfd
witness confirms the native main thread is waiting with the worker FD. Releasing
the query must produce a readable worker-FD return, accepted epoch2 observation
and unchanged native key count, before any further input is sent.

Snapshot replacement in this existing Recovery view resets its visible range.
Only after the input-free completion proof does the driver send End to reveal
the new issuer row. The OS peer then verifies old-reference retirement/refused
actions and the new same-full-ID row. Explicit reveal is not presented as
automatic viewport or focus preservation. Saved before/after snapshots separate
completion from the later End key. Its key-down can reveal focus before key-up
is consumed; no assumption requires both events in that first focus frame.

In close cases the peer retains an editable filter with a draft. Native Enter
closes/unmaps the UI while the original query remains held and its worker stays
alive. The OS peer rejects edits/actions on retired field/button/issuer
references before the driver releases the query. The late inventory is freed
exactly once on the GUI thread after view/window retirement; it is not adopted.
The original catalogue closes on its serialized worker. Each held host checks
its final descriptor count against entry, accounting for the intentionally
closed control-gate FD. Ordinary hosts and peers exit0; no final cleanup child
requires forced kill. Root and paired fixture User database bytes/rows remain
unchanged in all current-profile cases; legacy owners have the same original
read-only checks. Runtime checks remain metadata checks, not ProcessFD evidence.

The selected sanitizer profile instruments SDK/provider/original catalogue and
test host with ASan/UBSan and leak detection. OS libraries and the independent
libatspi peer are not all instrumented. This is no TSan/general race proof,
physical-input, human assistive or performance acceptance.

## Source, compiled controls and retained attempts

All32 [native inputs](recovery-root-v11-held-source-binding.json) match exact
Git blobs after CRLF→LF only, with no post-test code differences. Five wrapped
hosts and one lifecycle OS peer are built with recorded strict C17 compile/link
commands. They reuse the original configured profile flags and unchanged
provider libraries from the preceding builds; 36 used
library/provider/fixture files are matched by bytes/size against the previous
component archive manifest. No new complete CTest matrix is claimed for this
test-only extension; the prior21 component CTests retain their attribution.

Two actually compiled provider defects are detected by unchanged phase checks:
the worker posts its second completion without a wake byte, and an ordinary
provider joins the worker before retiring the GUI. The former fails FD-wakeup/
input-free completion; the latter fails unmapping before query release. Full
commands, variant sources, outputs and failure diagnostics are archived.

Attempt5 correctly observed input-free completion but incorrectly expected the
off-page issuer to be mounted after snapshot refresh. Its peer assertion and
driver timeout remain retained. Attempt6 explicitly reveals it only after the
completion proof and passes all28 cases. A first collector also assumed End-up
was already consumed at the End-down focus barrier. Its failure log remains;
the collector now checks the actual phase snapshots. Native assertions and
deadlines are unchanged; no failed attempt is counted as acceptance.

The [local receipt](recovery-root-v11-held-local.json) carries commands, results,
descriptor/poll witnesses, binary hashes and compiled controls. Raw process maps
and logs remain in the archive; compact map hashes preserve their binding.
Host maps exclude SDL/X11/EGL/Vulkan from software Wayland presentation.

Local artifact: `D:/NexaUI/out/nexaui-recovery-root-v11-held-20261108.tar.gz`,
13,522,755 bytes, SHA-256 `6e36066866faf3701ac5544c0cf858e48e7f333652128b04e73faf49e2c550fb`.
The [archive receipt](recovery-root-v11-held-archive.json) covers 825
files, 0 links and 310 directories, fully
verified after download. Reproduction also uses the existing component/SDK
archive SHA-256 `3abbf006b52920dce903f8ea11c1c895cc22b81fa549f90c27f74aa1e2e4da28`
and ordinary-native artifact SHA-256
`b2818c3e2291608b2cf1b53e5bf7deb404bfdeba0c7268cf3e3a4f3a67eeec84`.
Those frozen archives supply original source, installed prefixes, ordinary
fixtures and the test compositor. Borrowed font bytes are not redistributed.

## Still open

New-profile native clipboard and resize checks remain separate. The reset
viewport behavior is not changed by this test step. Actual current issuer/
image/ProcessFD/service admission, independent retirement and holder drain
require original host authority. Further packet6 Files/device/software/trusted
compositions, packets7–8 and final per-consumer performance/adoption/physical/
human gates remain open across103 IDs. Paused physical/human and Mac work was
not resumed. No1.0 release is made.

Independent live Aetheri was read-only observed at `fa5347264054b05e8af57e6d0a210008a6c849d1` during publication
and may advance independently. No Aetheri source/build/app/pin was changed.
