# Files read-only capture observations

Test-consumer source: `87d8336a0e7b823e982ab5eb14da7c89d9c53950`.
Installed SDK remains **0.1.33/source
`1e00a6f4f3a0ab3a7da1aa35a36f02679421366b`**. All fifteen app/child binaries
across the three profiles retain their earlier hashes from consumer
`e7138808ede25a08bcc568a093a2d7de425e56d4`. The native checks reuse these
artifacts. No SDK/application production source, API, version, dependency,
Aetheri source/pin/default or native host is changed by this work.

This closes the four known observation gaps from the
[full glibc checkpoint](files-full-report.md) for these fixed workflows.
The previous captures and differences remain historical evidence; broader
adoption and catalogue gates are unchanged.

## Change and original-test preservation

The original capture helper's fixed 80 ms delay does not establish that prior
input has reached an application frame. `run_native.py --observe-captures`
adds four independent native expectations; normal mode and the private AT-SPI
fixture are required:

| Original capture | Expected published native state |
| --- | --- |
| Body press/caption exclusion | Grid button focused |
| Inline grid folder | Name `work`, scalar caret 3, no selection |
| Mouse query selection | Search `seed`, caret 4, selection 0–4 |
| Minimum German naming | Name `draft`, caret 5, no selection |

The separately compiled `ime_atspi.c` reader reuses the original peer's bounded
discovery, state and text/caret/selection helpers. Its new focus mode only reads
button state. It links libatspi/GObject/GLib, not NexaUI or the Files model, and
sends no Action, text mutation, focus request or keyboard event. Native calls
retain the original one-second transport setting and 12-second parent limit.
The original 80 ms delay remains after the new checks, without increasing it.

`capture_observation.py` records three insertions: reader compilation,
expectations and result/hash metadata. The original reader still executes all
original assistive assertions. Without the option, the capture path is unchanged.
All three actual normal-driver adaptations, including SDK-selection changes,
reverse to the byte-identical original driver. The standalone
`check_capture_observation.py ORIGINAL_EXPORT` also verifies exact recovery,
unchanged delay and six missing/duplicate-boundary refusals. No original action,
assertion, deadline, EOF/CPU/frame guard or sanitizer is removed.

The software presenter still defines READY as submitted, not displayed. These
fixture-specific expectations are not a generic display fence or performance
budget; captured pixels supply the separate visible evidence.

## Executed checks and images

The same ordinary-user glibc VM and private compositor/registry are reused.
Shared Release, static Release and static Debug-O1 ASan/UBSan/LSan application
profiles each pass the complete original document/AT-SPI workflow with four
added native expectations, plus the complete 350-checkpoint IME/clipboard task.
That is **six native tasks, twelve positive expectations and 1050 IME checkpoints**.
Real original Terminal/editor input, exact `hello` saving, parent/pidfd checks,
create/rename/move, native chrome, cancellation, independent assistive actions,
disabled-provider refusal, short idle guards and clean close remain exercised.
IME tasks retain pressure, Undo/rejection, foreign clipboard, deliberate NoReply
rejection and font-startup checks. Independent readers use the original C17/O2/
warnings-as-errors compilation; sanitizer instrumentation belongs to app/SDK.

Three fresh private copies change only an expected-state literal: Grid→List,
`work`→`wok` with its intermediate caret, and selected query→no selection.
Each stops before its capture with reader exit 1 and the specific expectation
failure, without an outer timeout or transport-error diagnostic. All three apps
then close with `failed=0`. No process remains under the owned test root.
Earlier 81 CTests and lost-result workflows remain attributed to the unchanged
application build; they are not newly executed CTests in this observer increment.

All 396 new lossless captures are retained. All **264 complete shared/static/
sanitizer image pairs match**. Against the previous 396 corresponding captures,
exactly the four known sanitizer snapshots change to the preceding shared/static
reference pixels; the other 392 pairs stay byte-identical. Comparison uses full
RGB buffers with no crop, masks, normalization or density adjustment.

Four complete new sanitizer captures were visually inspected: Grid focus, full
`work` and caret, selected `seed`, and full German-view `draft` with Abbrechen/OK.
The shortened hint, English `Moved` status and primary-only unsupported-symbol
rendering elsewhere are unchanged. [Images/comparisons](files-observation-images.json).

## Evidence and remaining scope

[Native commands/results and negatives](files-observation-local.json),
[source/reader hashes and exact original-driver recovery](files-observation-source-binding.json),
[raw archives](files-observation-archive.json). All 612 positive and 259 negative
archive members are verified after transfer. Unchanged SDK/app/dependency evidence
remains separately bound to the full checkpoint; borrowed fonts and external
runtime/dependency libraries are not bundled in these new archives.

[CI42757503563](https://github.com/Lulus792/NexaUI/actions/runs/42757503563)
passes all seven required SDK/native/package jobs on the exact source, first
attempt. Full decoded logs and every reported passed-test line are checked:
Windows 148/147, macOS 124/123, Linux 272/270 and sanitizer 272/272 in both driver
orders; static Linux also passes its separate presentation repetition. Two
optional jobs are skipped; no CTest skips count as success. These regular SDK/
package checks are separate from the private native tasks. [CI receipt](files-observation-ci.json).

Affected evidence includes I01, A02/A04, P05 and Q02/Q03/Q06/Q08; all 103 status
columns remain unchanged. This covers the four named checkpoints, not arbitrary
capture synchronization, latency guarantees, human screenreader or current
Aetheri compositor acceptance. Wider Files interruption/provider/locale/IME,
normal adoption, remaining device/software/trusted compositions, packets7–8 and
physical/human/design/performance gates remain open. Aetheri continues independently.
