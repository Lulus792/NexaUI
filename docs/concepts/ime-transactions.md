# Opt-in native IME transaction boundaries

From 0.1.24, `nx_window_set_ime_transaction_events(window, 1)` enables
`NX_EVENT_IME_TRANSACTION_BEGIN` and `NX_EVENT_IME_TRANSACTION_END` around a
Wayland v1 commit or v3 `done` with a commit or nonempty deletion. This lets an
application validate the complete native operation before exposing any edit.
Previously, an all-filtered commit produced no TEXT event, so a preceding
deletion could not be distinguished from an independent delete request.

Call on the window's creation thread before its first successful IME enable.
The mode is then fixed for that window; setting the same value is harmless,
while changing it returns `NX_ERROR_STATE`. Invalid pointers or booleans return
`NX_ERROR_ARGUMENT`. X11, Win32, AppKit, unsupported platforms and a Wayland
window without text-input support return `NX_ERROR_UNSUPPORTED`.

Both markers carry identical `ime_target`, `ime_revision` and `ime_before`:

| Flag in `ime_before` | Meaning |
| --- | --- |
| `NX_IME_TRANSACTION_COMMIT` | The native batch contains a commit, including explicit empty/null input |
| `NX_IME_TRANSACTION_DELETE` | It contains a nonempty native deletion |
| `NX_IME_TRANSACTION_REJECTED` | Its commit contains filtered control characters or malformed UTF-8 |

Existing TEXT boundary flags, DELETE arguments and PREEDIT notifications retain
their meanings and order between the markers. The default OFF stream has no
new marker events. Old event enum values and `nx_event` layout remain unchanged.
Invalid native serials/delete ranges are still discarded, rather than exposed
as accepted edits. The v1 decoder also discards malformed UTF-8 before publishing
a transaction; the v3 path marks such a batch rejected. Preedit-only updates
remain ordinary PREEDIT notifications with no edit transaction.

For example, a v3 deletion plus a commit made only of filtered controls becomes:

```text
BEGIN target/revision, COMMIT | DELETE | REJECTED
DELETE_SURROUNDING target/revision, original byte counts
END   target/revision, COMMIT | DELETE | REJECTED
```

A valid empty commit has COMMIT without REJECTED and no TEXT scalars. A delete
without a commit has DELETE without COMMIT. Consecutive transactions retain
separate BEGIN/END pairs even when their target/revision match.

The application must keep a bounded buffer, reject nested/mismatched/incomplete
transactions and retain its own target, focus, selection, scope and model
revision. Apply accepted edits together only after matching END; reject the
entire operation when REJECTED is set. Discard buffered work on input/polling
errors, focus/context retirement or intervening commands. Queue exhaustion is
not a transaction boundary. These notifications do not provide automatic
atomic application, undo grouping, authorization or complete consumer adoption.

The protocol applies deletion before insertion; see the pinned
[text-input-v3 1.48 `done` contract](https://raw.githubusercontent.com/wayland-mirror/wayland-protocols/1.48/unstable/text-input/text-input-unstable-v3.xml).
The [wire tests](../../tests/test_wayland_ime_transactions.c) exercise real
private Wayland messages with opt-in and unchanged default streams. The
[v1 test](../../tests/test_wayland_text_input_v1.c) covers the fallback protocol.
The [installed C17 probe](../../tests/ime_transaction_consumer/CMakeLists.txt)
checks the new symbol and matching 0.1.24 header/library/package versions.

Files' existing separate consumer remains on its reviewed 0.1.23 experiment
until its complete buffering/application path is connected and tested against
this SDK. This API does not close that integration, real installed IME,
human assistive or physical display acceptance.
