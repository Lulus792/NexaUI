# Original Editor: complete log observations and real paste refusals

Consumer/test source: **`2dc70e97f50b58cba6ba4101cab150d2a5d08ac8`**.
Installed SDK: **0.1.33 / `1e00a6f4f3a0ab3a7da1aa35a36f02679421366b`**.
Original Editor: **`7bbe7042ecd48582b11ef7053a4dbeade6b02351`**.

This follows the [focus-paint checkpoint](editor-focus-paint-report.md). The
independent AT-SPI reader now rejects transport errors instead of reporting
them as semantic paste refusals. The native controller reuses complete decoded
log snapshots when the driver's owned regular log files have not changed.
Four complete native configurations now pass with the unchanged one-second
libatspi deadline and original EOF, input, frame, model and idle assertions.
SDK production code, public API/ABI, dependencies and version are unchanged.

## Diagnosis and bounded changes

The captured old reader called `g_clear_error` after a false `PasteText` return
and could subsequently print success despite a real transport timeout. The
new reader requires both false and no `GError` for refused/cancelled paste.
Its existing failure cleanup still frees the error and native references.

The optional `--paste-timeout-guard` holds the actual foreign writer open after
the original owner consumes an incomplete three-byte prefix. It deliberately
waits for the unchanged client deadline, records the open pipe and actual
error, and requires reader exit1 without a success marker. It then closes the
real writer and checks unchanged original text. This tests error classification;
it does not claim that a transport timeout revokes a pending server operation.
The guard is separate from every ordinary paste case.

The old reader binary from source `b0a254c` is a real negative control against
the final controller/app: it exits0 and prints success on the deliberate
timeout. The guard rejects that false success. The new reader exits1 as expected
in each of the four positive native workflows. Ordinary transport timeouts are
never accepted as refusals in these workflows.

The previous diagnostic recorded 2,366 full log calls taking27.634s. The new
reader cache keeps one complete text per observed log path and checks file
identity, size and nanosecond modification/change timestamps. A change still
uses the original full read and replacement decoding; nothing is tailed or
filtered. The pre-read stamp ensures that an append racing a read invalidates
the snapshot on the next observation. No reader descriptor remains open.
Cached strings remain owned by the test controller until it finishes.

Actual temporary-file differential tests cover18 observations: absent/empty
files, append, split/malformed UTF-8, line endings, truncation, same-size overwrite
with changed timestamp, replacement, explicit/default log switching, deletion,
recreation and a racing append. A fixed64-call unchanged-file workload performs
64 complete reads with the old function and one with the new function. The old
function fails that work-count regression after matching the same full text.
Existing coherent-frame and completed-focus tests remain, with nine refused
source-boundary counterexamples. These are controller-work results, not SDK
rendering, application-memory, end-to-end latency or SDL-comparison claims.

## Exact source and executed tests

The [source binding](editor-observation-source-binding.json) verifies110 Git
inputs,133 actual strict-C17 owner/OS-reader objects and28 generated C replays.
Installed SDK libraries and headers retain their verified1e00a6f bytes.
The [local receipt](editor-observation-local.json) binds these executions:

| Configuration | CTests | Complete native result | Captures | Ordinary native paste replies |
| --- | ---: | --- | ---: | ---: |
| Static Release | 23 | passed | 84 | 14 |
| Shared Release | 23 | passed | 84 | 14 |
| Debug-O1 ASan/UBSan/LSan | 23 | passed | 84 | 14 |
| Shared, attributes OFF | 21 | passed | 84 | 14 |

The exact-source root observer adds one CTest: **91 executed local tests**.
Four deliberately timed-out requests are checked separately from the **56
ordinary replies**. Every ordinary reply has `error=none` and elapsed time below
1,000,000us. App logs and all reader logs, including expected negatives, contain
no ASan/UBSan/LSan diagnostics. The original seven stream modes still provide
**104 forced pre-EOF frames**, with real foreign writer-pipe checks and no partial
model application. Original IME, independent text/selection/geometry/attribute,
8MiB document, copy/cut/paste, history/save, repeated-close and idle cases remain.

The exact OFF binary additionally fails when the reader demands omitted
attributes, with the actual default/run-value omission diagnostic. That failure
is distinct from both the old-reader false-success negative and the deliberate
transport-timeout negatives.

The [image receipt](editor-observation-images.json) compares336 complete native
captures in252 full cross-profile pairs: **246 equal, six different**. No masks
or tolerance are used. All six differences are in per-run temporary-directory
labels in the two Save captures; their full bounds/counts remain recorded. Nine
complete images were inspected, including both Save cases in all profiles and
the review reference. Filename caret/border and review Cancel focus are visible;
no further cross-profile focus/caret differences occur in this matrix.

The [CI receipt](editor-observation-ci.json) binds
[run42721235599](https://github.com/Lulus792/NexaUI/actions/runs/42721235599),
attempt1: all seven required jobs pass, with no CTest skips and two optional jobs
skipped. Full-log/file-change/work-count and existing frame/focus checks run on
Linux, Windows and macOS. The original C owner and native reader remain separate
Linux local/native evidence, not native Windows/macOS consumer acceptance.

## Preserved failures and independent development

The ordinary Release/OFF failures from the previous source remain in their
immutable archive. Current successes do not prove a unique cause of every prior
scheduling/timeout failure or repair the inaccessible older VM.

Two local fixture setup mistakes are also retained. A private freezer reused
two older helper filenames after completing the new Git export; both old files
were restored byte-for-byte from the verified archive before launching consumers.
An initial OFF preflight then ran before its independent Shared reader linked
and failed before starting Editor. Its intended-order scope text was disproved
by actual timestamps and corrected in a separate observation. The completed OFF
task uses a fresh output after verifying the dependency. No timeout/assertion
was relaxed or failed result overwritten. Some inherited wrapper scope strings
still say `owner55`; commands, executable hashes and object replays identify
the actual `owner70` builds and are the authoritative attribution.

At read-only original comparison `252d33ba586bd59c82a16f4270371301c27b3c6a`,
52 of53 consumed inputs are unchanged. Only `spec/nexaui-inputs.json` differs,
adding `model_support`; all consumed Editor/support code bytes remain identical.
That current checkout is not a newly executed app revision. No active Aetheri
checkout, pin or build was modified. The [archive receipt](editor-observation-archive.json)
binds new sources, binaries, objects, logs, captures, negatives, setup failures,
CI and docs and references unchanged prerequisite archives. Borrowed fonts are
excluded; their exact hashes remain in the receipts.

## Remaining scope

Continue the documented Files/device/software/component/trusted-system work
and subsequent content/VM/Physim/NexaCloud seams. Actual selected Aetheri
compositor, installed external IMEs/screenreaders, broader native interfaces,
physical/human/design/performance and normal adoption remain open. The cache
assumes the driver's owned regular log streams; it is not a general concurrent
filesystem snapshot API. All103 requirement IDs/statuses and remaining packets6–8
retain their scope. This checkpoint declares neither1.0 nor goal completion.
