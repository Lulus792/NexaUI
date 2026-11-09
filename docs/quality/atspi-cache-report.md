# AT-SPI cache metadata budget and original Editor

Source `ded94329aa859ffaedc23dc626816288055870cb`, 9 November 2026.
SDK0.1.28 development; no public API, ABI or version change.
Original Editor `7bbe7042ecd48582b11ef7053a4dbeade6b02351` remains frozen.
Catalogue SHA256 `9c932872caeba8007bf05b86f7c167b493260709b23c54d709222a5f19539e65`;
all103 requirements and the complete eight-packet goal remain active.

## Cause and correction

The previous bulk-cache preflight charged the native snapshot's allocation
capacity, including the entire document value and spare backing storage.
An8-MiB document therefore caused Cache.GetItems to return LimitsExceeded even
though its cache metadata occupied only1248 wire bytes. A small empty document
could inherit the same refusal after deletion retained the larger allocation.
The independent client could previously continue through lazy individual queries;
that fallback did not establish successful bulk-cache behavior.

The correction charges the fields actually serialized: the three object
references, accessible name and description, plus a conservative1024-byte
per-item allowance for fixed interfaces, scalars, containers and alignment.
The application and window items use the same accounting. Document values and
unused allocation capacity are excluded. All size updates subtract from the
remaining budget after checking the charge, avoiding additive overflow.
The4-MiB bulk-reply budget and8-MiB outgoing-queue guard remain unchanged.
The check finishes before constructing the reply's item array.

The protocol basis is GNOME's versioned
[AT-SPI2.60.7 Cache declaration](https://raw.githubusercontent.com/GNOME/at-spi2-core/2.60.7/xml/Cache.xml)
and the [D-Bus0.43 wire format](https://dbus.freedesktop.org/doc/dbus-specification.html#message-protocol-marshaling).
Cache items contain no document value. For this serializer, six interface names
are the maximum; their fixed bytes, fourteen string length/terminator/alignment
allowances, four struct alignments, two array prefixes, two indexes, role and
two state words fit within1024 bytes. Actual bus/path/name/description bytes
are charged separately; the existing2048-byte envelope allowance is retained.
This is conservative admission accounting, not an exact-size serializer.

## Executed installed consumers

Release-static, Release-shared and whole Debug-ASan/UBSan profiles each pass
15 focused SDK CTests, an installed package consumer and two independent native
AT-SPI tasks:45 SDK CTests,3 packages and6 native tasks altogether.
The OS reader links system libatspi/D-Bus, not NexaUI or the application model.
It explicitly sends GetItems over a private connection, checks the exact wire
signature, marshals the reply and finds the expected Text object/application/name.
Its existing1000ms native deadline is unchanged.

Each normal task observes four cache items and1280/1248/1280 wire bytes before
large replacement, at the complete8-MiB document, and after complete deletion.
The metadata task publishes3072 additional names of1536 bytes: the name payload
alone exceeds4MiB. It requires the actual LimitsExceeded wire error and then
reads the complete3073-child root, last name and document through individual
queries. The expected libatspi bulk-warning in this oversized fixture is
preserved. Its ordinary document is only four scalars.

The original Editor is rebuilt against each installed SDK profile. All45 owner
CTests pass;96 owner/OS-reader objects are recorded. All three full native tasks
pass with216 complete pixel captures, original keyboard/IME, Unicode editing,
Undo/Redo/save, withheld-input close review, retained-reference refusal,
full8-MiB operations, output moves/resizing, zoom/styles and normal final close.
The independent reader performs39 explicit original cache queries, one in each
of13 phases per profile, including large and read-only documents. Their complete
wire messages range from5976 to8292 bytes. Each final1.2-second idle guard
observes zero CPU ticks and zero new frames; this is not an energy benchmark.

The [source binding](atspi-cache-source-binding.json) records1067 SDK inputs,
94 original-consumer inputs and8 standalone-fixture inputs. All53 consumed
original inputs remain unchanged at independently checked Aetheri HEAD
`b71d816aaf692476d17a662df39073d8b74722bf`. All21 generated C inputs replay
exactly. The unchanged producer's63 dependencies and binary hash are checked.
Five current complete raw captures equal the previously visually reviewed
Unicode/review/large/contrast/light images. There is no active Aetheri source,
build, pin, default or service change.

## Regression, failures and retained evidence

The new raw query fails against installed SDK9ad027a at the full8-MiB document
with LimitsExceeded. A separately compiled candidate mutation omits only the
name/description charges. Its oversized bulk query wrongly succeeds, and the
new metadata reader rejects it. Positive SDK prefixes are never mutated.

The [check ledger](atspi-cache-checks.json) preserves initial wrapper permission,
quota, socket-path and collector failures. The ASan SDK initially fails during
installation after its successful15 tests. The original ASan task initially
fails in the test compositor during its first capture. Same-build continuations
pass after verified inactive artifacts are retired; the original guards,
deadlines, sanitizer settings and rendering are retained. A too-long retry
label is rejected by the existing socket guard and replaced with a shorter
fresh output label. Failed attempts are not counted as successful native tasks.

The [local matrix](atspi-cache-local.json) and
[archive ledger](atspi-cache-archive.json) bind actual sources, objects,
libraries, commands, logs and complete captures. Raw archive members are
verified by type/mode/hash; retirement is restricted to verified inactive
members, retaining installed SDK prefixes and frozen source exports.
All seven required exact-source CI jobs pass in
[run42554502042](https://github.com/Lulus792/NexaUI/actions/runs/42554502042).
The [CI ledger](atspi-cache-ci.json) binds seven complete decoded logs:
Windows static/shared144/143 tests, macOS120/119, Linux263/261 and both
sanitizer driver orders263/263. Linux static also passes its separate lifecycle
repeat. Both new native cache/document tasks execute on both Linux builds and
both sanitizer orders without CTest skips. Installed package, close-request
and add_subdirectory consumers pass on all six platform builds. Two optional
workflow jobs are skipped. This CI validates the SDK/platform suite; the full
original-owner matrix above is separately executed against the frozen export.

This closes the earlier original-document bulk-cache refusal for the tested
scope. Native document clipboard/geometry/attributes, broader native text/cache
behavior, new app-document AX/UIA tasks, installed real IMEs/screenreaders,
physical/human/design/performance gates and ordinary Aetheri adoption remain
open. Remaining packet6, packets7–8 and all103 requirements are not complete.
Version1.0 still requires the user's explicit approval.
