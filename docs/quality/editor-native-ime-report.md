# Original Editor: native document IME integration

Consumer source `52d59e45c67b59c10b05b9c51bb8015b00ad9334`, 8 November 2026.
Original Editor `7bbe7042ecd48582b11ef7053a4dbeade6b02351`;
installed SDK0.1.27 source `47ec4e640a8a0f54bc8a09d1fe825b57956df6d7`.
Catalogue SHA-256 `9c932872caeba8007bf05b86f7c167b493260709b23c54d709222a5f19539e65`.

## Result and authority

The [optional original Editor consumer](../../examples/36_aetheri_editor_consumer/README.md#native-document-ime-variant)
now connects the installed SDK's native IME transactions to the original
document/controller/history. Both `EDITOR_BUILD_IME_MODEL` and
`EDITOR_BUILD_NATIVE_IME` must be ON. The default stays OFF; native ON with model
OFF is explicitly rejected. There is no SDK implementation/API/version change,
new document model, active Aetheri modification, pin update or default-owner switch.

An app-owned65,536-byte collector admits only the focused document's current
target/revision and all six original controller-token fields. Only a matching
complete END may replace text through the prior atomic original-model seam.
Rejected/filtered, malformed, stale, wrong-boundary, interrupted, oversized and
out-of-order batches preserve the complete original document and history.
Queue exhaustion cancels an incomplete transaction rather than inventing END.
Own accepted queued commits advance the original token while retaining their
advertised native epoch until the native queue drains. Ordinary input, focus,
review, selection/document changes and pointer gestures retire the old context.

Transient preedit uses existing layout/selection/clip APIs. Nonempty preedit and
accepted IME edits cancel the original pending-paste operation. A composition can
continue through its own acknowledged commit, with selected or hidden caret;
high-contrast selected glyphs remain black on yellow. Long preedit scrolls inside
its own clip and cannot overdraw the original prefix. Candidate geometry remains
logical through text scale and backing density. Surrounding text reuses the
original render snapshot, is capped at4000 bytes, and preserves scalar/CRLF
endpoints and the full selection. A larger selection publishes identity only;
the original controller still owns replacement. The original filename field
retains its own native path. Missing transaction support preserves ordinary editing.

The read-only composition semantic snapshot is not document editable accessibility
or independent AT-SPI acceptance. Unsupported preedit has an IME-owned notice that
does not erase a newer original save/file message.

## Executed native and model checks

| Configuration | CTests | Complete native task | Full captures |
| --- | ---: | --- | ---: |
| Static Release, both options ON | 13/13 | Native document IME plus original task | 57 |
| Shared Release, both options ON | 13/13 | Native document IME plus original task | 57 |
| ASan/UBSan/LSan, both options ON | 13/13 | Native document IME plus original task | 57 |
| Static Release, both options OFF | 11/11 | Not rerun in this configuration | — |
| Same static ON binary, no IME protocol | Reuses the ON build | Ordinary original task | 30 |

No local skips. Actual compile commands prove C17 and warnings-as-errors for
84 ON-profile objects,25 default objects, six producer objects and four negative
objects. The sanitized consumer uses the matching fully sanitized installed SDK;
leak detection and halt-on-error stay enabled. Original fault wrappers, deadlines,
complete document assertions and the short idle guard are unchanged.

The collector test adds full marked-batch acceptance, malformed/stale/interrupted
rejection, byte/scalar/CRLF ranges, bounded allocation failure, AltGr and queued
separate undo entries to the twelve prior original/model regression groups.
The complete native-v3 task checks real private-compositor callbacks and:

- Uncommitted `Grüße`, accented commit and original Undo/Redo; one done callback
  containing a commit and a continuing combining-mark preedit; hidden caret.
- Atomic delete/Unicode/empty/delete-only/combining replacements, selected-text
  replacement and two coqueued commits with separate original undo entries.
- Full rejection of filtered delete+commit and partial UTF-8 deletion while
  retaining an earlier valid queued commit.
- Replacement of `aX́b` with `aq́b` at the final grapheme caret, without an
  intermediate deletion moving the insertion position.
- A1000-character preedit, full before/after original-prefix RGB equality,
  logical in-view candidate and fresh surrounding text after accepted commits.
- Actual Escape release and a newer context epoch before a fresh commit; old
  queued commits rejected across Escape/Tab, original indentation retained,
  toolbar focus disables IME and document focus restores a fresh context.
- Original Open of a6003-byte document,3999-byte surrounding tail, native edit
  to6005 bytes, identity-only context for selection over4000 bytes, full selection
  replacement/Undo and exact original-worker save bytes.
- Original close/review/Cancel followed by continued edit, move, resize,
  maximize/restore and final native close/Save. The post-Escape1.2-second guard
  allows at most two CPU ticks and no new frames; it is not an energy benchmark.
- German200% high contrast and150% light text at2x backing density, including
  preedit/candidate/commit/Undo while retaining original clean-state behavior.

All201 positive PPM captures have complete decoded size and byte-hash checks.
Six [full lossless previews](editor-native-ime-images.json) were actually inspected:
[preedit](editor-native-ime-preedit.png), [compound callback](editor-native-ime-compound.png),
[hidden caret](editor-native-ime-hidden.png), [clipped long preedit](editor-native-ime-clipped.png),
[high contrast](editor-native-ime-contrast.png) and [2x density](editor-native-ime-density.png).
Every PNG was independently decoded and every RGB byte compared to its source.
These images establish controlled fixture appearance, not final human/design acceptance.

Four separately compiled functional defects fail at their intended oracles:
mutation before END, admission of FILTERED, missing preedit clipping, and omitted
post-edit surrounding refresh. The clipping defect is caught by the complete
original-prefix image comparison; stale context is caught by the actual processed
provider state. Compiler failure is not counted as a detected functional defect.

## Source, archive and CI binding

[79 exact Git inputs](editor-native-ime-source-binding.json) comprise53 original
and26 repository files, with physical hashes and explicit LF normalization.
Consumer build bundle19 and executed driver21 differ only in the native-flow
oracle. Producer16 dependencies match the final source; its17 original inputs
bind to frozen `d56d69523c33a48648af343e7d36526f84518c90`.
All19 generated C outputs replay byte-for-byte from checked inputs across the
three profiles, default host and nested producer pipelines. The consumed53
original committed files were rechecked against independently advanced Aetheri
HEAD `c03484b0e64604c433ef3c3951725eddf62cef4a` and remain identical.

[Local receipts](editor-native-ime-local.json), [checked counts](editor-native-ime-checks.json)
and [archives](editor-native-ime-archive.json) bind648 selected final artifacts.
The11,880,799-byte final archive has SHA-256
`bf160ca5f0d3e7a4255a65bf8dba03778852c56f05ba30efd9e937103365a1c7`;
all1377 files, four links and587 directories were verified. The progress archive
retains earlier builds/failures; the prior SDK and original Editor archives bind
the reused three library binaries, fonts and producer provenance.

All seven required SDK CI jobs pass in
[exact-source run42529058431](https://github.com/Lulus792/NexaUI/actions/runs/42529058431):
Windows static/shared142/141, macOS118/117, Ubuntu259/257 plus the separate
static presentation repeat, and sanitizer259 in each of two discovery orders.
Optional Windows-ASan and fuzz jobs are skipped. The [complete job/log receipt](editor-native-ime-ci.json)
binds all seven decoded logs and successful installed close-consumer steps.
These jobs test the SDK/installed ABI; external original Editor coverage is the
local matrix above. No physical Mac session was used.

## Preserved failures and limits

VM user quota exhausted during captures despite filesystem free space. A bounded
64-MiB probe reported EDQUOT after55,574,528 completed bytes; smaller probes had
passed. A diagnostic producer stopped with SIGBUS before READY, without its
stdio trace. Exact kernel fault attribution is not proven. Only archived inactive
build roots were retired after full file/mode/hash/tree and live process checks;
five retirement receipts account for417,801,980 bytes. Root privilege was used for
process reads only, with permanent UID/GID1000 drop before artifact checks/deletion.
Installed SDK prefixes, original exports and current builds remain. No global
quota, mount, driver or cache setting changed. The same original fallback binary
then passes; earlier failed receipts remain explicit rather than overwritten.

The first producer configure used an obsolete wrapper missing the immutable
Editor-export option; the corrected existing wrapper passed. Earlier driver
oracles observed an old candidate or stale rendered frame before the processed
new context. Final barriers correlate original model revision/cursor as well as
provider state. Visual review found an actual long-preedit prefix-overdraw bug;
the separate preedit clip fixes it and the compiled defect proves the new oracle.
ASan driver19 admitted an intermediate modifier-only seed context; driver20 sent
a fresh commit before the Escape release retired that context. Driver21 requires
a strictly newer document revision after seeding and actual Escape-release/new-epoch
provider state before continuing. All three final21 tasks pass. No deadline,
sanitizer, CPU or redraw allowance was relaxed. Old failed matrices are retained
and are not described as completely passing.

Still open: independent editable document accessibility, installed real IMEs,
delayed foreign clipboard/composition interaction, live output migration and
candidate restoration after manual offscreen scrolling. Vertically offscreen
caret currently disables canvas IME until visible again. Ordinary Aetheri
owner/pin adoption, physical/human/design/performance acceptance, the earlier
Software ASan AT-SPI Read-action instability, remaining packet6, packets7–8 and
the complete103-ID scope remain active. This checkpoint is not a1.0 release.
