# Save destination ownership over the original Files worker — 2026-11-07

Consumer source: `cc27e805202c27a239b5e8565601830578e4dd30`.
Installed NexaUI remains `ac0fb3b3b3aa53b2a4de927ec52a06deb33d5c36`, version
0.1.23; original application source remains Aetheri
`27de5cb228da0f37264fad64d964ad7a660d210d`. This advances D07/P05/Q02/Q04
within packet 6. It is the destination contract for the forthcoming Save UI,
not completed native Save adoption or a completed packet.

## Reused contracts and behavior

The optional [picker consumer](../../examples/32_aetheri_file_picker/README.md)
adds `FP_SAVE_NAME`, `FP_SAVE_SELECTED` and `fp_reply_take_save` to its existing
bounded original-model worker. These are application adapter contracts, not new
public NexaUI SDK APIs. There is no SDK API/ABI, version, external package,
live Aetheri source, consumer pin or 1.0.0 change.

`FP_SAVE_NAME` copies one literal leaf and its byte length, validates it using
the original `ae_files_create_name`, and looks it up relative to the original
held directory. It does not reconstruct a path from labels or current filesystem
names of the ancestor stack. The original 255-byte UTF-8/name/control restrictions
apply without truncation. Six additional original create/editor sources and
headers are hash-checked alongside the earlier five inputs. The original create
validator is reused without extracting or duplicating its implementation; its
original editor/document dependencies also provide the actual handoff tests.
This optional application model does not become a dependency of NexaUI's core.

The typed-name lookup intentionally sees names outside the displayed filter,
hidden entries and names created after the snapshot. Only ENOENT produces
`FP_SAVE_NEW`. Access/resource/other I/O errors remain failures. Existing regular
files produce `FP_SAVE_EXISTING` with an owned O_PATH/CLOEXEC witness. Symlinks,
directories, FIFO and other types are refused without content reads or following
links. `FP_SAVE_SELECTED` instead calls the original observed-row selection and
open checks: changed named objects fail, and arbitrary existing raw filenames
remain byte-exact even when they cannot be entered as a new UTF-8 name.

`fp_reply_take_save` transfers the held parent, optional existing witness, literal
leaf and separate original escaped label once. Ordinary scalar/batch take helpers
refuse these replies. Unclaimed destinations close with message/channel disposal.
Both operations require the original adapter's current observed state; stale
requests do not perform lookup/validation or acquire a new destination. Failed
admitted attempts supersede older commands as before. Worker access remains
separate from UI objects and from the application's document controller.

NEW is a transient absence observation: a consumer must use create-only
admission and reject a name occupied later. EXISTING requires explicit overwrite
review and an independently rechecked witness before the application's own
controller may write. No selection is a grant, save attempt, clean-content
acknowledgement or durable receipt. Neither this worker nor its take helper
writes or retries an uncertain operation. The original controller retains its
document/save/unknown semantics and its documented ordinary-user limitations;
the witness is not an atomic conditional rename against hostile concurrent writers.

## Executed checks

Fedora 44 VM, Linux 6.19.10, GCC 16.2.1, CMake 4.3, Python 3.14.3, the same
three installed SDK profiles and frozen original `d56d695` compositor used in
the [multi-picker report](file-picker-multiple-report.md). Static Release,
shared Release and static Debug ASan/UBSan with active leak detection each pass
**5/5 CTests**, with strict conversion/shadow warnings as errors and no skips or
sanitizer suppressions. Final receipts are `evidence/commands-input3.json` and
`evidence/{release,shared,asan}-input3.xml` inside the frozen bundle.

The new actual-filesystem Save test checks:

- Unobserved/stale folder refusal, copied command name/key lifetime, original
  invalid-name rules, 255-byte ASCII/multibyte limits, oversized byte counts and
  full interior-NUL/invalid-UTF-8 refusal.
- Held-directory acquisition failure and actual EACCES, plus directory/FIFO/
  ordinary/dangling-link refusal, with no destination descriptors leaked.
- Filter-independent late/hidden targets, original 255-byte non-UTF-8 selection
  and its full 1,020-byte escaped label, a separately typed literal backslash
  name, O_PATH/CLOEXEC ownership and refusal of cross-kind/double transfer.
- A selected existing object replaced by another file: the original editor's
  `ae_editor_file_open_selected` rejects the mismatch and both files retain
  their content. A formerly absent destination occupied later is rejected by
  the original `ae_editor_file_new`, without adopting or replacing it.
- A retained target through navigation plus renaming/replacing its old parent
  path: the original app controller creates/saves exactly `saved` through the
  held parent; the decoy stays empty. That controller's successful write, clean
  state and uncertainty are inspected separately from target selection.
- Unclaimed reply disposal, pending channel teardown, original worker-thread
  affinity and complete final process-descriptor balance.

Three final private source counterexamples each build successfully and fail the
specific new Save test: remove NOFOLLOW, bypass original name validation, or
ignore expected state. Their source copies, binaries, commands and exact failing
assertions remain under `mutants-input3` and `evidence/mutants-input3.json`.
Earlier passing test versions and earlier counterexamples are retained; final
claims use input3. No product test failed in this step.

Two independent libatspi/native regressions pass on the final unchanged host
binaries, with host/peer exit 0 and no forced cleanup or sanitizer diagnostics:
maximum-entry shared German/200%-text/small/high-contrast multi-owner (15 peer
stages), and sanitizer light/small single-owner with controlled IME and complete
result reading (8 stages). These verify the previous visible modes after the
reply-layout change; they do **not** exercise a visible Save field or overwrite
dialog. The maximum multi case retains the known Cache.GetItems budget warnings;
individual Accessible/Selection queries pass. The earlier unresolved AT-SPI heap
incident and human bulk-reader/cache/performance acceptance remain open.

## Frozen evidence and resource scope

`D:/NexaUI/out/nexaui-picker-save-target-20261107.tar.gz`:
SHA-256 `fdd07a196d77b4e0506a9c8bf833d8349aba0ead9435a50e5ed3ada3631ea302`,
70,028,910 bytes, 82,069 regular files, 94 links and 2,165 directories plus
`archive-manifest.json`. Guest copy:
`/tmp/nexaui-picker-save-20261107/nexaui-picker-save-target-20261107.tar.gz`.
Local full archive hash/entry-size/hash/mode/directory/link verification passed
for every manifest entry after transfer.

The complete current in-process command is 1,088 bytes. Reply header is 133,784
bytes, each copied row 1,320 bytes, and the maximum logical reply is 5,540,504
bytes; the transferred Save target is 1,296 bytes on this Linux/x86-64 build.
The reply header grows by 1,032 bytes from the previous batch/multi source.
These are exact structure sizes, not allocator high-water, process RSS or
performance acceptance. No file descriptor/process/system limit is raised.

The frozen source is independently compared with the exact committed archive,
allowing only line endings; all eleven original inputs are checked. The bundle
also retains the three SDK prefixes, full original source archive, all tested
sources/builds/corpora/logs/receipts, actual peers/runtime inputs and negative
variants. FIFO fixtures are represented by path/type/mode in `special-nodes.json`;
ordinary file bytes, symlink targets and directory/file modes are manifest-checked.
The first collector followed a deliberately dangling fixture link; the next
archive exposed unmasked stat type bits in manifest modes. Their helpers and
unaccepted archives are retained. The final collector preserves links and stores
permission bits matching the serialized tar entries; tests/SDK behavior were not
changed to fix packaging. Guest/source clocks and global storage settings are
unchanged; only owned extracted source mtimes were normalized for building.

## Remaining adoption gates

Next connect this contract to the existing visible picker/owner: filename editor,
unmodified raw-row binding, explicit complete overwrite review, cancel/focus
restoration, native keyboard/IME/assistive behavior, resize/themes and held/late
completions. Save result selection must stay separate from application-owned
write/progress/conflict/unknown state and original-binding versus export policy.
Native child parenting/actual OS close, real services/grants/reconnect, other
platforms, physical/human and measured performance acceptance remain required.
The full 103-ID catalogue and packets 6–8 remain active. Aetheri can independently
evaluate this ordinary-user destination adapter; native Save UI is not yet
recommended for adoption from this worker evidence alone.
