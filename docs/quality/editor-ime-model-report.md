# Original Editor: atomic model prerequisite for native IME

Consumer source `33511dade22194f10ec8399460fe93d51f1decf6`, 8 November 2026.
Original Aetheri Editor `7bbe7042ecd48582b11ef7053a4dbeade6b02351`;
installed SDK0.1.27 source `47ec4e640a8a0f54bc8a09d1fe825b57956df6d7`.
Catalogue SHA-256 `9c932872caeba8007bf05b86f7c167b493260709b23c54d709222a5f19539e65`.

## Behavior and boundary

The [separate original Editor consumer](../../examples/36_aetheri_editor_consumer/README.md)
adds an optional `EDITOR_BUILD_IME_MODEL` variant. It generates two original-model
files from the checked immutable export. A new application-owned entry admits
one byte-range replacement only when all six original direct-input token fields
match: controller identity, interaction, document revision, cursor, anchor and
selection state. It reuses the original document preflight, gap buffer, line
index and delta history. Ordinary original entry points retain their existing
grapheme-boundary policy.

IME ranges can replace a complete scalar inside an old grapheme. The original
engine computes the caret from the complete final text before allocating or
mutating anything, avoiding an intermediate delete that snaps the insertion
point to a different grapheme. One replacement forms one undo group. Success
collapses selection and consumes the original token, including a successful
no-op gesture. Invalid scalar/CRLF endpoints, controls, limits and allocation
failure preserve bytes, cursor, revision, selection, history and correlation.
Payload buffers must not alias document storage.

This is a T03/T04 prerequisite. The document canvas still has its original native
input path. The option does **not** assemble native BEGIN/END events, display
preedit, publish surrounding text/candidate geometry or provide editable
accessibility. The owning adapter must perform those connections and admit the
focused window/document/target/epoch before calling this entry. No second editor
model, SDK API/version, dependency or application authority is introduced.
The default option is OFF and compiles the original models directly. The active
Aetheri checkout, pin, launch owner and default editor are unchanged.

## Executed checks

| Configuration | CTests | Native original Editor regression | Captures |
| --- | ---: | --- | ---: |
| Static Release, model option ON | 12/12 | Passed | 30 |
| Shared Release, model option ON | 12/12 | Passed | 30 |
| ASan/UBSan with leak detection, model option ON | 12/12 | Passed | 30 |
| Static Release, model option OFF | 11/11 | Not rerun in this configuration | — |

No local skips. Each ON profile runs all eleven original regression groups and
the new atomic-model test. The OFF profile separately verifies the original
model build and eleven tests. All78 ON-profile and25 OFF-profile C objects have
recorded C17/warnings-as-errors compile commands. Sanitizer flags cover the whole
consumer and matching installed SDK; leak detection and halt-on-error remain on.
Original fault wrappers and the45-second save-work deadline are unchanged.

The new test verifies seven replacements with exact caret/Undo/Redo results,
including a retained combining mark after a new line, regional-indicator
re-pairing, insertion of a combining mark, complete CRLF deletion/insertion and
ordinary ranges. It rejects each corrupted token field, a foreign controller,
NULL arguments, stale gestures, partial UTF-8/CRLF, malformed/control text and
line/count limits without changing state. Selection collapse, separation from
ordinary typing history and token consumption on no-op are checked. Failures
at each of the three actual allocation sites preserve a selected document and
its pre-existing redo branch; the successful allocation case then supports Undo.

Two actual separately compiled model defects fail at their intended assertions:
removing the interaction-generation check admits an altered token; bypassing
history recording loses the single undo entry. These are real model objects
linked into the same atomic test, not synthetic failure hooks.

All three native runs preserve the full original editing, selection, save and
external-conflict tasks plus the prior [retained-window close workflow](editor-close-report.md):
two compositor close requests during one review, Cancel, continued editing,
move/resize/zoom/restore, then native close and original Save worker. All90 PPM
captures have complete decoded size and byte-hash verification. These are native
regressions with the adapted model, **not native document IME acceptance**.
Short idle guards remain functional checks, not energy/performance measurements.

All seven required SDK jobs pass on the exact consumer source in
[CI run42521413365](https://github.com/Lulus792/NexaUI/actions/runs/42521413365):
Windows static/shared142/141, macOS118/117, Ubuntu259/257 plus the separate
static presentation repeat, and sanitizer259 in each of two discovery orders.
Optional Windows-ASan and fuzz jobs are skipped. These are SDK regressions and
installed ABI consumers; the external original Editor/model project is checked
by the local configurations above. [Complete job/log receipt](editor-ime-model-ci.json).

## Provenance and remaining work

[Local commands, objects and results](editor-ime-model-local.json),
[source binding](editor-ime-model-source-binding.json),
[checked counts and remaining gates](editor-ime-model-checks.json) and
[archive receipts](editor-ime-model-archive.json) bind72 staged inputs
(53 original plus19 repository files) and431 selected artifact records.
All three generated model transformations replay byte-for-byte. The committed
README has only a post-stage documentation addition. The53 consumed committed
Aetheri inputs were rechecked against its independently advancing HEAD and remain
identical to the accepted original snapshot.

The new local archive preserves both model attempts, build roots, original
inputs, generated sources, successful/negative binaries, native logs/captures
and VM drivers. Every1050 file, seven links and815 directories was verified.
One selected original-test fixture link is explicitly bound to its archived
internal target; the three reused SDK libraries are individually bound in the
prior SDK archive. The reused private producer and fonts retain the prior
Editor archive/source bindings. These local archives are not release assets.

Next: connect complete native transactions to this model seam; preserve same-epoch
queued commits; publish bounded surrounding context; keep preedit separate from
document/history; compute the candidate rectangle from current logical layout
after scroll/font/scale changes; cancel stale focus/tab/review input. Document
editable accessibility, independent assistive clients, ordinary Aetheri adoption,
installed IME and physical/human/performance acceptance remain open. Packet6,
packets7–8 and the remaining103-ID scope are unchanged. The earlier Software
AT-SPI instability remains a separate unresolved finding.
