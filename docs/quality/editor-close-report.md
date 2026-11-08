# Original Editor: retained window and native close review

Consumer implementation `a494ddd453d57b733fbc62bfbe51b3b3b72d4bee`,
8 November 2026. Original Aetheri Editor:
`7bbe7042ecd48582b11ef7053a4dbeade6b02351`. Installed SDK0.1.27:
`47ec4e640a8a0f54bc8a09d1fe825b57956df6d7`. Catalogue SHA-256:
`9c932872caeba8007bf05b86f7c167b493260709b23c54d709222a5f19539e65`.

## W01 behavior and ownership

[Example36](../../examples/36_aetheri_editor_consumer/README.md) compiles the
original direct Editor, document/workspace/file/save models and tests from a
checked immutable export. A separately generated native host replaces the
sticky first-close latch with the existing SDK take/resolve contract. It keeps
the latest native token during the original review, resolves cancellation when
that close sequence is cancelled, and accepts only after the original workspace
finishes closing and saving. A later native close supersedes the token without
restarting an already active document review. Forced native failure retains the
existing failed-teardown path; normal renderer/model/save-worker ownership is
preserved.

The existing original header gains caption and edge gestures through public
move/resize/capability APIs. The SDK retains native serial/held-press validation;
the original zoom button, document input, rendering, dialogs and file effects
remain in use. Diagnostic-only wait entry/return and Escape-release records
expose actual owner-loop boundaries without adding SDK hooks or synthetic events.
No SDK API/version change and no active Aetheri source, pin, launch/default-owner
change is part of this step.

## Executed acceptance

| Profile | Original regression groups | Full private native task | Verified full captures |
| --- | ---: | --- | ---: |
| Static Release | 11/11 | Passed | 30 |
| Shared Release | 11/11 | Passed | 30 |
| ASan/UBSan with leak detection | 11/11 | Passed | 30 |

No local skips. The original groups cover graphemes, document, direct input,
workspace, asynchronous save work, modal/history/search/selection, local file
effects and the original editor view. Existing fault wrappers and the original
45-second save-work deadline are retained. All75 actual consumer C objects and
six producer objects have recorded C17/warnings-as-errors commands. The compiler,
link flags, actual objects, generated hosts and executables are retained in the
[local receipt](editor-close-local.json).

All seven required core SDK jobs pass on the exact consumer source in
[CI run42516928629](https://github.com/Lulus792/NexaUI/actions/runs/42516928629):
Windows static/shared142/141, macOS118/117, Ubuntu259/257, and sanitizers259
in each of two discovery orders. Ubuntu static also passes its separate
presentation repeat. These are SDK regressions and package consumers; the
external original Editor project is exercised by the three local profiles
above. Optional Windows-ASan and fuzz jobs are skipped.
[Exact job/log receipt](editor-close-ci.json).

Each native task preserves the original save/conflict/selection/document and
style cases, then exercises this full retained-window sequence:

1. Type `retained` into an original untitled document. Send two actual xdg close
   notifications during the same original unsaved-work review.
2. Cancel with real Escape input. Resolve the newest SDK token, retain the same
   native window/document/caret, then process the final Escape release and reach
   the original blocking native wait.
3. Observe1.2 seconds with no additional frames and the existing budget of at
   most two CPU ticks. All three final profiles observe **zero ticks and zero
   frames**. Wait IDs change126→127,126→127 and113→115 respectively; normal
   protocol housekeeping is allowed to return from polling. This short guard is
   not a statistical performance, energy or long-run resource result.
4. Continue native text entry, drag the caption40×30 logical units, resize by
   60×40, and continue editing. Original window ID1 remains unchanged.
5. Activate the original zoom button, await actual committed1920×1080 geometry,
   then restore the1160×780 window at80,70. The deliberately clicked zoom button
   owns focus; unchanged document/caret is checked without claiming unchanged
   canvas focus after that explicit action.
6. Send another real native close. Use the original Save dialog and save worker
   to publish exact `retainedxz` bytes into `native.txt`, accept the latest SDK
   token and exit0. Earlier `changed` and external-conflict file bytes remain
   intact.

The private producer observes actual scene coordinates and committed xdg
geometry, not only request submission or stored normal-window positions. Its
inherited socket admits C/Q only for a sole mapped ordinary window and refuses
DRM/nested ambiguity. The older checked producer's original integer-density
branch is taken unchanged from the current checked Editor fixture to preserve
the later200% output-style task. This is disposable test policy, not a normal
desktop controller or authority.

Two separately compiled real host defects are rejected at the intended
Cancel/retained-window boundary: retaining the superseded token, and answering
Cancel with ACCEPT. Both run through actual compositor callbacks and the
original UI; neither failure depends on a missing diagnostic hook. Their sources,
objects, commands, native logs and earlier original workflows are preserved.

## Images and source provenance

All90 captures have complete decoded PPM size/hash checks. Four lossless full
PNG previews were opened and reviewed; every encoded RGB byte also matches its
source after decoding. [Native review](editor-close-review.png),
[retained editor after movement/resize/zoom](editor-close-retained.png),
[German200% high contrast](editor-close-contrast.png) and
[light150% text at2× output density](editor-close-density.png) show complete
controls and retained/selected text for this corpus. The current opaque
development presentation is not the accepted glass design or a physical/human
acceptance. [Image receipt and review limits](editor-close-images.json).

Sixteen repository inputs bind to `a494ddd`;53 original Editor inputs bind to
`7bbe704`;17 producer inputs bind to
`d56d69523c33a48648af343e7d36526f84518c90`. Actual compiled bundle7 and executed
driver bundle8 differ only in the uncompiled native-flow oracle and README; all
C sources/headers, build inputs and generation logic are identical. Every final
input has an individual Git blob/hash binding. The SDK builder-declared revision
remains empty; the existing SDK source/artifact evidence binds actual bytes.
See [source binding](editor-close-source-binding.json) and
[check summary](editor-close-checks.json).

The final [verified archive and retirement receipts](editor-close-archive.json)
bind all391 selected native artifacts and the three consumed SDK libraries.
The full native archive is22,843,970 bytes, SHA-256
`84c3a6bf8dadc286d5d2ee7d09348d740b4d5bc3f197250f04a1e9dd8970427f`.
Every member's bytes/mode and complete directory/link set was verified. It also
contains earlier failures, source bundles, original producer inputs, fonts with
licences and native drivers. An initial archive omitted inherited producer sources;
the final full archive supplies those17 inputs and passes all artifact bindings.
CI logs and host verification/retirement drivers have separate evidence storage.
Local archive paths are not published downloadable release assets.

## Preserved failures and limits

The first export omitted the original grapheme fixture. The first producer used
a geometry getter absent from the pinned installed wlroots0.20 headers. Both
input/API boundaries were corrected before successful final runs. An initial
producer SIGBUS preceded UI launch; diagnostic output encountered EDQUOT.
The same binary progressed after verified inactive artifact retirement, but the
exact kernel fault attribution is not claimed. Only archived captures and one
inactive SDK ASan build were retired:480,919,669 bytes. All installed prefixes,
original sources and active work remain available. Privilege was used for the
previously unreadable process inspection, followed by a permanent UID/GID1000
drop before file verification or deletion; no quota/mount/cache policy changed.

Earlier native oracles confused Wayland edge10 with public SDK bottom-right5,
observed maximize before its committed dimensions, and required an uninterrupted
poll despite allowed protocol returns. One initial ASan idle sample measured an
extra post-Escape frame. Final measurements use the observed Escape-release/
sleeping-owner boundary and retain the original no-frame/CPU/deadline budgets.
The original failures remain distinct from successful final evidence.

W01 now has the missing installed original-editor controlled workflow evidence.
Normal Aetheri owner/admission/pin adoption, external IME and editable-accessibility
integration, real screenreader/design review, physical monitors/input/resume and
performance/resource budgets remain open. The prior Software AT-SPI instability
is unchanged. Remaining packet6, packets7–8 and all103 catalogue IDs retain their
full scope; this step does not declare W01 or the overall goal fully complete.
