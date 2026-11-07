# Original selected software observations

This optional C17 consumer starts the software/component composition with real
`ae_catalogue_resolve_app` results. Its trusted Root owner supplies the source
identity/context, recipient account/UID and an explicit set of installation IDs.
The presentation model receives only copied metadata. It owns no catalogue path,
SQL, launcher, device service or permission capability.

This step contains the read producer and immutable presentation model, not a
native software-management window. Worker scheduling, the virtualized list,
filter/details, native input/assistive publication, artifact dependency joins and
production service transport remain subsequent integration work. The SDK's
existing channel, list/tree, text, action and software-window APIs are retained.

```sh
cmake -S examples/34_aetheri_software -B build-software \
  -DCMAKE_PREFIX_PATH=/path/to/installed-nexaui \
  -DAETHERI_SOURCE_DIR=/path/to/immutable-original-aetheri
cmake --build build-software --target software_model_test
ctest --test-dir build-software --output-on-failure
```

The original Aetheri inputs reuse example 33's 67-input check at
`46e2a21ef979d9a76db2a8487d1de167a6a79ef7`, normalizing only CRLF to LF.
Keep that source immutable and independent from Aetheri's active checkout.
Original build, OpenSSL and SQLite requirements remain in force; they belong to
this optional application consumer, not to the NexaUI SDK. NexaUI package/header
consumption here is not evidence of native NexaUI rendering or platform linkage.

## Source and observation contract

`sw_read` is an owner-side producer to schedule on the application's worker.
It must not be called as blocking I/O from a GUI frame. It uses the original
serialized Root handle and copies the target/context inputs before original
reads. No method starts a runtime, migrates, writes/caches a user binding,
changes a gate, dispatches or authorizes an operation. The host still owes the
original protected-source, broker, recipient and MAC/volume admission.

Each original query observes one eligible active app: active account/machine
scope, committed active generation, verified artifact and a separate gate state.
Gate-open metadata is not a launch ticket or process/readiness observation.
The result retains complete epoch/runtime/boot/install/publisher/account/scope/
generation/artifact identities and their original revisions.

The selected set is **not catalogue enumeration**. Empty success means no
selected targets, not no installed software. A per-target `-ENOENT` means no
eligible active app was observed for this recipient. It must not be relabeled
as removed, uninstalled, explicitly denied or a complete absence of the target.
Other original errors reject the whole batch and preserve caller output; a
successful prefix cannot be published as a complete batch. Original contexts
and revisions are checked before/after, and every successful row must agree.
These guards create no cross-query transaction, provenance or future authority.
An unstarted original runtime produces unavailable state, not confirmed emptiness.

The public discussion draft contains `ListComponents`/`ListInstalls`; it explicitly
does not establish an installed service. This adapter uses the implemented
selected-install C contract. Device control, full inventory/permissions,
process/resource observations and install/update/removal admission need their
actual owner/service contracts. It never builds those inventories with private
SQL or infers them from the recovery inventory or manifest path list.

## Presentation ownership

The selected target set is copied, checked for duplicates and canonically sorted by full
16-byte IDs. Up to 100,000 declared targets are accepted; oversize inputs fail
as a whole. This is a declared model bound, not a measured application budget
or proof of 100,000 real installed apps. Query I/O costs remain external.

Each owner binds source/recipient/target identities for its entire lifetime.
Changing those requires a new owner. Accepted tickets strictly increase;
superseding a request never disposes a worker's channel reservation or payload.
Only the current ticket can move an entire validated response into the model.
Source drift, recipient/target substitution, revision rollback, changed contents
under an equal revision, malformed rows and allocation failures preserve the
old snapshot and pending request. A negative original error completes as
unavailable while retaining the previous observation and selected full ID.
Unavailable target rows carry no stale successful-install metadata.

Snapshots are owner-thread reference-counted and remain valid after a new
observation or the owner's destruction. Selection is read-only inspection of a
complete install identity; it grants nothing and remains on a selected target
even when that target becomes unavailable. GUI semantic IDs must not substitute
row positions for installation identity across snapshot/context changes.

The tests use private original Root v2–v10 catalogues and a User owner. Five
selected IDs cover two eligible account/machine apps, an ineligible account
target, a preparing target and an unknown ID. Read-only cases require identical
database bytes/rows. One separate case explicitly changes the original runtime;
old context is refused and a new one can observe again. Owner/model checks
exercise stale tickets, exact context/recipient/keys, immutable revisions,
partial-read/OOM failures and retained snapshots. A separate declared 100,000-row
model rejects a hidden malformed row; it is not original backend inventory.
