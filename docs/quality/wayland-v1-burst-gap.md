# Open Wayland-v1 consecutive-commit input loss

**Unresolved SDK defect; no native Physim acceptance or fix is claimed.**
Published main remains `014449f6682b97bc9b0d559c669aa1c8477f2b60`.
Native consumer `795812b4f09f469677c2c3e33a763ed9986623f2` and deterministic
regression `eb575d116c5a8fabe1e11876511e8cea127e8372` are work-branch checkpoints.
Both use installed SDK0.1.33/source `368cd38292498bd538964ba514ff02e0e9924358`;
the runtime implementation has not been changed. The new root regression is
expected to fail that SDK, so this branch is not ready to fast-forward main.

## Actual native discovery

The optional Physim host uses the same original model and view as the preceding
[model checkpoint](physim-model-report.md). It adds ordinary Wayland/software
presentation, native Numeric IME binding, visible model validation/status,
Replace view, independent libatspi/X11 observation and actual screen captures.
The host has no X11 presentation dependency and the peer links original Physim
without NexaUI. All27 original Physim inputs and its application checkout remain
unchanged by this work.

Exact-source build311 passes16 CTests: four each in static Release, shared
Release, ASan/UBSan/LSan and default fixture-OFF. Default builds discover no X11
and contain no native fixture executables. These passes do not prove the new
native workflow. In formal run312, Release and Shared each pass full,
ime-cancel, ime-replace and ime-close; ASan full passes, but ASan ime-cancel
fails. ASan ime-replace/ime-close are **unexecuted in that formal matrix**, not
skips or successes. The whole matrix is not green.

The failed task uses real private Weston Simple-IME, compose:ralt, native Text
selection and real keys2/period/5. Its protocol trace records:

```text
set_surrounding_text("1.5", 3, 0); commit_state(7)
commit_string(7, "2")
set_surrounding_text("2", 1, 1); commit_state(9)
commit_string(7, ".")
commit_string(9, "5")
set_surrounding_text("25", 2, 2)
```

The unchanged peer rejects the resulting field because it expected `2.5`.
It does not invoke Apply after that failure; the original physics remains at
generation1/length1.5. This is input loss under ordinary asynchronous serial
ordering, not an ASan diagnostic or an outer timeout. Adding per-character
waits would avoid the demonstrated ordering and is not an accepted fix.

The actual local text-input-v1 XML describes commit serials as the latest text
input state known by the sender. The backend currently advances
`ime_surrounding_serial` on every changed surrounding snapshot, including a
snapshot produced by an earlier accepted IME commit. In `nx_wayland_ime_commit`,
the older period serial then fails `snapshot_current` and is discarded before
a normalized text event reaches the Numeric field.

## Deterministic regression

`tests/test_wayland_text_input_v1.c --burst` extends the existing controlled
v1 server rather than introducing another window/input stack. It advertises
a Numeric field `1`, commits `2`, waits until the app reports `12`, then sends
the period with the earlier serial known before that native edit. The required
draft is `12.` and a following current-state commit should produce `12.5`.
The unchanged library instead produces `12` at the first assertion.

Reproduction316 builds that test against the actual installed library in all
three profiles. Each burst run exits1 at the draft assertion with cleanup;
the unchanged legacy and transaction modes exit0 in the same executable.
Thus the new failure is deterministic in Release, Shared and Sanitizer, and
the existing stale/withdrawn/password/transaction controls remain available.
This is a regression test and localization result, **not a repaired SDK**.

[Source/object/command/native-failure ledger](wayland-v1-burst-gap-local.json)
and [independent archive readback](wayland-v1-burst-gap-final.json).
`nexaui-wayland-v1-gap317.tar.gz` retains477 regular members,18,211,272bytes,
SHA-256 `a3f617f51ff490a7979e1d43f1d93ca22684256779b6f58048399a79043a55b7`.
The archive includes frozen consumer/original/regression source, objects and
executables, actual successful/failed native runs, traces, PPMs, earlier pilot
failures, commands and the local protocol XML. Every member was read back and
hashed twice. The unexecuted ASan cases remain explicitly recorded as unexecuted.

## Earlier consumer repairs retained

Initial native302/304 failed after complete semantic detach disconnected the
last assistive window. Diagnostic304 independently confirmed the old field was
Defunct and rejected focus/edit; the fresh field could not be found through the
retired application connection. The host now uses the existing Login consumer
handover pattern: build and publish the replacement context before retiring the
old view. A staging frame's viewport lease is completed before discarding it.

Native306 full and ime-cancel passed; ime-replace failed because Simple-IME owns
F6 during composition. The visible Replace control now permits an independent
host action while preedit exists. Pilot308 passes all four Release cases.
Pilot310 additionally types the valid number through real IME commits instead
of assigning it through EditableText, and all four Release cases pass. Formal
matrix312 is the first Sanitizer exposure of the consecutive-serial race. All
earlier source/logs and the failing formal trace are retained; they are not
converted into final acceptance by a later successful run on another profile.

## Required correction and remaining gates

The next SDK change must distinguish a surrounding revision caused by a proven
accepted IME edit from an arbitrary programmatic edit, selection change, editor
replacement, withdrawal or sensitive-input transition. Merely widening the
serial range, matching equal text, waiting between keys or retrying a stale
commit is insufficient. Keep stale deletion, old-target, replay, password and
focus/context guards strict. A possible private built-in-editor feedback path
needs an explicit before/after revision witness; that is a proposal, not an
implemented or reviewed contract. Low-level/app-owned editor behavior must be
accounted for separately rather than inferred from built-in Numeric success.

After correction: pass the deterministic regression and original v1/v3 controls,
rebuild/install exact SDK profiles, rerun the native Physim matrix without
weakening its inputs/checks/deadlines, run relevant SDK/platform CI, then bind
new artifacts/revision before promoting main. The current root CMake adds the
burst test so a future candidate cannot silently omit it.

R11 remains partial. Production Physim runner/workers and approved UI,
scene/camera/picking/object accessibility, additional datasets/plot types,
NexaCloud adoption, GPU/physical/human and declared performance gates remain
open. No Aetheri/Physim/NexaCloud source or pin, global tool/driver setting or
1.0 release is changed. All103 requirements and ordered delivery packets remain
active. Aetheri continues independently.
