# Software measured action captions and compact focus — 2026-11-08

Consumer/test source: `750c7dce8e1d09d68f41bba5ea096934ac77e6ca`.
Preceding view: `70408963bd6f52ea43cb07a82c28f06cb79bcee1` (view bytes unchanged since8b29872).
Installed SDK: `c4f467d85b0147944d2456f5e7602a39ac18e579`, NexaUI0.1.23.
Original catalogue: `46e2a21ef979d9a76db2a8487d1de167a6a79ef7`,73 checked inputs.
Private compositor producer: `d56d69523c33a48648af343e7d36526f84518c90`, reused from the checked Files/Software window fixture.
Catalogue SHA-256: `9c932872caeba8007bf05b86f7c167b493260709b23c54d709222a5f19539e65`.
Scoped L01/L02/L05/L11/I01/I05/A02/A06/P04/P05/Q02/Q03/Q04/Q08 evidence, packet6.

The [Software consumer](../../examples/34_aetheri_software/README.md) now reflows
complete action captions using existing text measurement and button APIs. Old
equal columns remain when they fit. Otherwise the bounded set of at most four
actions occupies measured rows, preserving keyboard/semantic order. No SDK
implementation, API/ABI/version, application model/worker or live Aetheri source/
reviewed pin changed. The private view/cache layout changed and must be rebuilt.

## Behavior and bounds

The [preceding native image](images/software-window-filter.png) shows large German
captions breaking inside words. The [new filter image](images/software-label-filter.png)
shows complete single-line Anwenden/Zurück followed by Schließen on its own row.
Widths come from the real borrowed font, with existing control padding/gaps.
There are no language-specific guessed widths. Cached caption layouts survive
normal frames; resize/context retirement releases them with the existing caches.

The header reserves usable content space. If captions or content cannot fit,
the existing compact Close-only presentation stays inside the client; its X icon
is measured and reduced only when needed to fit tiny bounds. Its accessible name
remains Close/Schließen. Enlarging restores the intended control and draft.

An added compact-Escape case reproduced a focus defect in the initial reflow
candidate: after canceling the hidden filter, enlargement still targeted the old
query ID. The view now retains the mode change's intended focus, including mode
changes that occur while compact. Both component and native tests cancel while
compact, return to Filter focus on enlargement and reopen the retained draft.

## Executed verification

| Checks | Static Release | Shared Release | Debug ASan/UBSan/LSan |
| --- | ---: | ---: | ---: |
| Existing view/components/history-worker/history-links CTests | 4/4 | 4/4 | 4/4 |
| EN/DE foreign clipboard/close/resize native tasks | 6/6 | 6/6 | 6/6 final |
| Ordinary controlled IME/navigation | 1/1 | 1/1 | 1/1 |
| Held original query wake/close regression | 2/2 | Not repeated | Not repeated |
| Native capture repeat | 1/1 | Not repeated | Not repeated |

There are24 final native cases and362 checkpoints. Seven resize/capture cases
each add five compact-Escape checkpoints to the previous327-checkpoint matrix.
All final positive hosts/peers exit0 without forced termination. Original fixture
Root/User database bytes/rows remain unchanged. The installed SDK and ordinary
host/worker/input/renderer APIs are reused; no new SDK core CTest run is claimed.

The component button-call observer checks every submitted header caption is one
line and fits its actual bounds, including padding. The view test exercises
English16px/dark, German32px/2×/high contrast, English16px/1.25×/light and
German48px/high contrast. Both original Root schema2 and10 run these four styles
on all three profiles:24 style/schema/profile cases. It also checks compact
240×160 capacity, draft retention, Escape mode change, focus, stale requests,
errors and the existing synthetic100,000-target declared maximum. That synthetic
maximum is not an original complete software inventory or performance budget.

The independent native peer checks the large German action rows remain above
the filter. It observes old field/button retirement and refuses late requests
across compact cancellation. Native captures retain all three original output
sizes at1×; these are not output migration or fractional-scale checks. The three
full output0 images were losslessly converted and inspected:
[filter](images/software-label-filter.png), [compact](images/software-label-compact.png),
[restored](images/software-label-restored.png). Complete captions, bounded focused
Close and restored draft/caret are visible. [Image binding/review](software-label-images.json).

Carrier/SDK/compiler provenance is retained from the [lifecycle](software-lifecycle-report.md)
and [window](software-window-report.md) checkpoints: the unprivileged private
Linux x86-64 VM and original headless compositor. Selected sanitizer builds
instrument SDK/provider/original catalogue/native hosts; OS libraries and the
independent OS peer are not all instrumented. No TSan, hardware, human screenreader,
final design or performance acceptance follows from these cases.

## Failure evidence and open stability finding

Two actually compiled preceding-view controls fail the final checks: the view
test rejects its multiline caption; the independent native peer rejects its old
action-row geometry. Source/object/binary/logs remain separate from positive
hosts. The intermediate compact-focus failure and correction are retained too.

The first ASan native plain clipboard case failed before the new resize section:
the independent peer's Read action returned false at checkpoint3, followed by
the parent's ACK3 timeout. The host remained in observed/list state. The same
sources, binaries, flags, checks and deadlines pass in a fresh private case;
all remaining final cases pass. The original failed directory/commands/logs are
retained. Its cause is unproven; it is an **open AT-SPI stability finding**, not
attributed to scheduling or declared fixed by this layout change. See
[structured finding and exact evidence](software-label-checks.json).

The first component attempt hit storage quota. Only after complete checked
backups were exact inactive task-artifact roots retired with hash/size/mode/path/
UID/process guards. Later completed test outputs were likewise archived before
retirement. No global quota/cache setting, timeout/assertion/sanitizer, or live
Aetheri checkout was changed. Intermediate attempts keep their own attribution.

## Source, artifacts and remaining scope

[Source binding](software-label-source-binding.json) matches95 staged inputs to
exact Git blobs after CRLF→LF only, with the README-only post-execution change
explicit. [Local results](software-label-local.json) and [host checks](software-label-checks.json)
bind283 artifacts,73 original inputs, actual compiled objects, six checked caption
logs and both real prior-view controls. Reused SDK/original/runtime inputs match
their preserved archives; five private clipboard runtime files were re-hashed.

The final33,569,607-byte host archive is
`D:/NexaUI/out/nexaui-software-label-final-20261108.tar.gz`, SHA-256
`c95d75a1e99b76ef5b0f429d9b9b86ccb540c0f414dc9e904304a9dd1dbd99b8`.
Separate fully checked final Release/shared/sanitizer archives retain raw component
captures and fixtures; previous lifecycle/window archives retain reused inputs.
[All archive receipts](software-label-archive.json) include the intermediate and
storage-backup boundaries. No raw native image is replaced by its PNG preview.

Next: actual output/scale/workarea transitions and investigation of the retained
AT-SPI failure. Broader fonts/locales/IME/clipboard, complete inventory/management,
device/process/trusted-owner services, normal adoption and final hardware/human/
performance/design acceptance remain open. Remaining packet6 and packets7–8,
all103 IDs and the independent Aetheri development scope are preserved. No full
catalogue row or1.0 release is declared from this checkpoint.
