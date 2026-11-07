# Original Aetheri recovery observations

This optional installed-SDK C17 consumer begins packet 6's component/recovery
presentation with an owned immutable observation model. It uses the original
`ae_catalogue_recovery_inventory` result and free function. There is no second
inventory query, SQL implementation, recovery actor or authority API here.
The native list/detail view and its keyboard/IME/assistive acceptance remain
the next composition step; this foundation is not a complete Recovery app.

The trusted Aetheri owner remains responsible for opening its catalogue on its
single owner thread, retaining the original lock/context and obtaining a complete
bounded read transaction. `view_model.h` receives only its copied identity,
context, accepted query ticket and owned original result. It receives no
catalogue handle or path. A production IPC/worker host still has to correlate
and deliver replies using the existing application channel; this example does
not introduce another channel or an owner service.

Successful completion transfers the complete original allocation once. A view
retains a snapshot reference and can inspect it after newer observations or
owner destruction. The original data, including nested pointers, is borrowed
read-only. All calls and reference counts belong to one application thread.
Selection binds the complete record kind and opaque 16-byte ID; a removed
selection clears instead of following a row ordinal. Record IDs are not assumed
to be UUIDs, labels or authorization tokens.

Unobserved, reading, observed and unavailable are distinct. An unavailable read
retains the previous observation and original errno. A successful empty inventory
is observed empty metadata. It proves neither external holder drain nor global
readiness. No operation can be replayed, admitted, cancelled or settled through
this model. A new query ticket supersedes an old reply, without cancelling its
work or releasing a channel reservation.

The local acceptance policy requires the same original owner/epoch/boot/runtime,
nondecreasing catalogue revision and identical records/schema at equal revision.
Revisions remain observations, not mutation fences or future readiness proofs.
Changed context or explicit schema rebinding needs a fresh owner/session and
native view/provider retirement. Native retirement is not implemented here yet.
Malformed, stale, inconsistent or allocation-failed candidates stay caller-owned;
the pending ticket and prior snapshot survive. The caller frees rejected original
results with `ae_recovery_inventory_free`. It may then explicitly report a read
failure for the current ticket; rejected replies do not silently retry work.

## Reproduce

Use a fresh immutable archive of original Aetheri
`46e2a21ef979d9a76db2a8487d1de167a6a79ef7`, never its live working checkout.
Configuration checks 66 original build/core/catalogue/schema/fixture inputs,
normalizing CRLF to LF only. Original OpenSSL 3.5.9–3.5.x, SQLite and Python
requirements remain in force for this optional consumer. They are not NexaUI
core dependencies, and no consumer pin or live catalogue is changed.

```sh
cmake -S examples/33_aetheri_recovery -B build-recovery -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/path/to/installed-nexaui \
  -DAETHERI_SOURCE_DIR=/path/to/immutable-original-aetheri
cmake --build build-recovery --target recovery_view_model_test
ctest --test-dir build-recovery --output-on-failure
```

The test driver creates private 0700 directories/0600 databases with original
Python fixtures and explicit migrations. Fourteen view-only owner/schema cases
compare every result record with the original independent Python reference and
require identical database bytes and table rows afterward. Root v2–10 and User
v2–4/v7–8 are exercised; base kinds 1–11 are populated as applicable. Later
schemas are exercised with empty added tables, not actual closure/handoff/unit
custody workflows. The source's original 65,536-record bound is retained; a full
maximum-size populated owner is not benchmarked here.

Two separately marked cases deliberately invoke the original backend's runtime
replacement on those private owners. A genuinely newer query must be refused
by the old presentation session and accepted by a newly bound one. These are
explicit backend mutations, excluded from the view-only byte-equality claim.
The tests also inject malformed observations, ticket races, allocation failure,
equal-revision inconsistency and full-key/selection/lifetime adversaries. The
adversarial record edits are test inputs, never asserted as actual backend work.
