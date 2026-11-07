# Original Files model on a picker worker — 2026-11-07

Consumer source: `9a42f0133aca070e193ed7b09ba206ef749e02ab`.
The installed SDK implementation remains
`ac0fb3b3b3aa53b2a4de927ec52a06deb33d5c36` (0.1.23). The external Linux C17
[example](../../examples/32_aetheri_file_picker/README.md) builds five hash-checked
inputs from original Aetheri `27de5cb228da0f37264fad64d964ad7a660d210d`:
its Files model and original UTF-8/grapheme support. It uses the existing
`nx_sim_channel`; it neither replaces that model nor adds an SDK dependency.

This is the data-owner foundation for a file picker. The native dialog, editable
filter/IME, accessible list/breadcrumb composition and normal Aetheri adoption
are still open. The existing original Files application remains synchronous.
No live Aetheri sources, defaults, SDK pin, public SDK API or version changed.

## Ownership and state

One pthread exclusively creates, reads, changes and destroys the opaque original
model. Start duplicates the caller's root descriptor before returning and does
no directory scan on the UI thread. Copied fixed commands contain no UI pointers
or callbacks. A two-slot channel and nonblocking/CLOEXEC wake pipe provide bounded
queued work and an owner-loop descriptor. Receive drains the pipe before taking
completions; callers drain messages until ticket zero and dispose each one.

READ observes current state. Other commands require the last observed adapter
state; each admitted attempt advances it, including failed original operations.
Old-state commands return Changed without replay. Original snapshot revision is
reported separately. View/request keys are copied; the future UI must validate
both its active key and the channel ticket before adopting results.

Complete replies own copies of original rows, raw names, escaped labels, query,
hidden state and all held ancestor labels. Full-snapshot ordinals preserve the
original filter membership and order; display labels never become filenames.
Original limits remain 4,096 entries and 64 navigation levels. On the checked
x86-64 ABI, the header is 67,208 bytes, each row 1,320 bytes and the maximum reply
reservation 5,473,928 bytes. Reservation precedes model operations; safe optional
shrinking follows projection. Caller-retained replies are outside channel-slot
accounting and must be disposed promptly. This is not a process RSS budget.

Pick reuses the original regular-file inode/type checks and yields an ordinary
held directory descriptor, O_PATH witness and literal leaf. Take transfers them
once; otherwise message destruction closes them, including orphaned completions.
They are not file contents, a grant, StorageRef or durable authority receipt.

Stop rejects new requests, cancels queued commands and permits active original
I/O to finish without touching a retired GUI. Join requires Stop. Arbitrary
filesystem I/O has no asserted shutdown deadline. Destroy joins the worker,
disposes abandoned replies/witnesses and releases the channel, pipe and root.

## Executed checks

Fedora 44 guest, Linux 6.19.10-300.fc44 x86-64, GCC 16.2.1, CMake 4.3.0 and
Python 3.14.3. Static Release, shared Release and static Debug ASan/UBSan/LSan
each build under `-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror` and
pass **1/1 CTest**, comprising six actual filesystem/core groups:

1. Navigation, hidden/filter state, symlink refusal, held renamed ancestors versus
   a decoy, substituted inode refusal, old copied snapshots and once-only witness
   transfer surviving model destruction. A 255-byte invalid-UTF-8 filename retains
   its complete 1,020-byte escaped valid-UTF-8 label.
2. Atomic rejection at 4,097 entries, full admission at 4,096, last ordinal 4,095
   retained through filtering, filtered-out selection refusal, malformed reply
   size rejection and closure of unclaimed selected descriptors.
3. All 64 held levels, refusal of level 65, root-bound ancestry and descriptor
   balance after releasing deeper levels.
4. Explicit query-byte validation, including interior NUL/invalid UTF-8, the full
   255-byte valid query, invalid oversized commands and READ rediscovery of hidden
   state. Empty success remains distinct from a failed read.
5. Repeated wake/drain cycles, held real Refresh with 16 core Busy/Cancel frames,
   old-state Enter refusal, keyboard Cancel and context destruction before active
   I/O is released; queued cancellation, Stop/Join and descriptor balance.
6. Caller root closure after Start, orphaned successful Pick descriptor cleanup
   and invalid initial root failure with zero state/revision.

Link-time test wrappers observe more than 36,000 original-model calls in each
final profile and **zero on the caller thread**. Production worker code contains
no gates or test hooks. Core Busy/Cancel checks do not prove native window, IME,
AT-SPI or human interaction acceptance.

Three isolated current-source counterexamples each exit 1 at the intended check:
removing the state guard lets stale Enter navigate; omitting reply-descriptor
cleanup fails the EBADF check; constructing the model on the UI thread fails the
zero-constructor-calls check. A changed original `src/files/model.c` is refused
by CMake's source guard. The initial strict-build failure assigned an unsigned
clicked flag to int; an explicit boolean comparison fixes it without suppression.
Initial logs, binaries, input archives and earlier cases are retained.

## Frozen artifacts and reproduction

Local bundle: `D:/NexaUI/out/nexaui-picker-worker-20261107.tar.gz`.
SHA-256: `adf202eace8a1ec57c05fd97b04f3cdd183133950594e717d5664d848847d579`.
Size: 21,859,896 bytes; 50,702 regular files, 14 symbolic links and 1,041
directories. The copied bundle was independently verified on Alpine-Dev,
including raw non-UTF-8 filenames, empty deep directories, modes, file hashes
and link targets. The bundle contains source/archive equivalence, installed SDK
artifacts, five original-input hashes, the original full archive, commands,
JUnit, binaries, logs and negative cases. Production SDK equivalence covers
159 inputs; the historical extra root CTest registration is explicitly accounted
for rather than treated as an SDK implementation change.

Installed SDK library hashes (Release/shared/sanitizer respectively):

```
7d1491df1adbcdaf4e481eec90ea6f7632f929770aebc6f956cd0a67ccfc7541
c8c4b66ee58ef9445b866ca1e87514083d455a37e502fbb8dc6791ba2c620679
c6f4340cd626607d5b16f6c08860ab2888e5fba1fc4897dd40fa77b6da47d5b9
```

Build/run commands are in the example README; `shipping-commands.json` retains
the executed profile flags and outcomes. The original source archive SHA-256 is
`ab6f36cf98b45fb36f006173e7432a6e676f2ed53003e3747d2839d33421e812`.

## Requirement scope and next work

This advances L06/I07/D07/P04/P05/Q02 at the original-model ownership and copied
observation boundary. It does not close those requirements or add native Q04
evidence. Next: compose the actual ordinary native file picker over these
results, correlate current ticket/view identity, retire stale native objects,
provide keyboard/IME/filter/list/breadcrumb/error/Retry/Cancel behavior and test
it with an independent assistive peer. Directory/multiple/save choices, device
and software models, real services/permissions, durable reconciliation, full
application adoption, performance and physical/human acceptance remain open.
