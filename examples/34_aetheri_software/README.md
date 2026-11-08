# Original selected software observations

This optional C17 consumer starts the software/component composition with real
`ae_catalogue_resolve_app` results. Its trusted Root owner supplies the source
identity/context, recipient account/UID and an explicit set of installation IDs.
The presentation model receives only copied metadata. It owns no catalogue path,
SQL, launcher, device service or permission capability.

The example composes the read producer, immutable presentation model and bounded
owner worker into a native Wayland selected-app observation window. It has a
virtualized list, local filter and full metadata details. Optional original
manifests join through exact observed artifact/component/publisher identity and
show complete declared dependency IDs. Full inventory and production management/
service adoption remain separate work. The SDK's existing channel, list, text, action, semantics, clipboard, IME
and software-window APIs are reused; no SDK implementation/API/version changes.

```sh
cmake -S examples/34_aetheri_software -B build-software \
  -DCMAKE_PREFIX_PATH=/path/to/installed-nexaui \
  -DAETHERI_SOURCE_DIR=/path/to/immutable-original-aetheri \
  -DSWG_TEST_FONT=/path/to/font.ttf
cmake --build build-software --target software_model_test software_worker_test \
  software_view_test software_components_test software_history_test \
  software_history_fixture software_history_worker_test software_history_links_test \
  software_native
ctest --test-dir build-software --output-on-failure
```

The trusted startup host can append `--history PROFILE_HEX` (repeat up to 64)
alongside existing `--manifest INSTALL_ID ABSOLUTE_PATH` options, after the
selected installation IDs. `PROFILE_HEX` is the complete 768-character canonical
original execution profile, decoded by `ae_execution_decode` before any window
is opened. It is supplied by the owner, not selected or constructed by the UI.
The worker copies it before starting and reads original historical process
metadata after each successful selected-app observation. No GUI file read,
process lookup or current-process capability is introduced.

The History button opens full launch/profile identities and read-only details
for both historical roles, with the original revisions, digests, timestamps and
64-bit recorded identifiers. Enter opens a profile; Escape/Back restores its
tree focus, then the History button. Page Up/Down reaches all detail text.
Historical enrollment never establishes a current process, image, resource,
ownership or permission state. Missing configuration and unavailable observations
remain explicit, and do not become an empty process inventory. Unexpected
history errors leave the independently valid selected-app result usable.

`software_history_worker` exercises four unchanged original fixture scenarios,
copied inputs, independently owned replies, bounded admission, cancellation and
GUI retirement, plus three view styles and maximum-profile/string stress.
`run_history_native.py` uses `software_history_atspi` as an independent OS peer
with a private compositor/bus; its available, unavailable, corrupt and unconfigured
cases verify native keyboard focus, full read-only text, paging, retired objects,
explicit refresh and normal close. These fixtures contain synthetic historical
metadata. They do not establish live processes or human/physical acceptance.

The original Aetheri inputs reuse example 33's 67-input check at
`46e2a21ef979d9a76db2a8487d1de167a6a79ef7`, normalizing only CRLF to LF.
Keep that source immutable and independent from Aetheri's active checkout.
Original build, OpenSSL and SQLite requirements remain in force; they belong to
this optional application consumer, not to the NexaUI SDK. Worker tests use the
installed SDK's channel and UI-context lifecycle. Optional font tests exercise
the view and headless pixels; the separate native workflow below exercises the
Wayland software host and OS accessibility transport.

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

## Bounded owner worker

`software_worker.h` is application-private. `sww_start` copies an explicit
protected absolute Root source path and identity. The worker alone opens, reads
and closes the original catalogue; the handshake returns its original context
without scanning the selected apps. Opening/get-info may block this handshake.
Thread separation does not create broker, IPC, provenance or authority checks.

`sww_submit` copies the complete scope and selected target array before admission
and wakeup. The existing `nx_sim_channel` reserves two command/completion slots;
capacity returns `NX_ERROR_LIMIT`, with no ticket or input ownership transfer.
Each completion carries both its channel ticket and copied view/request key.
The UI must check both before adopting a result. The model independently checks
its current ticket and exact source/recipient/target set. Changing views does not
cancel or free a channel reservation. `sww_reply_take` detaches one successful
result once; every received message, including stale and error replies, needs
`nx_sim_message_dispose`. A transport/allocation status is separate from a
reply's original negative errno. Do not interpret either as confirmed emptiness.

The nonblocking close-on-exec pipe is a wake hint, not the result queue. Before
sleeping, repeatedly receive until the channel returns no ticket. Receiving drains
the pipe before taking a completion, so already queued completions must be drained
without waiting for another byte. The native host uses the existing
`nx_window_poll_with_fd`; no periodic polling or new SDK API is required.

Stop rejects new admission. Work already dequeued can finish; queued work completes
as `-ECANCELED` without a new original scan. Retire the view and its publication
before stop/join. Worker code contains no UI context, callback or semantic object.
Joining waits for original I/O and has no filesystem deadline. After joining,
destroy drains undispatched owned responses, destroys the channel and closes the
pipe. Successful detached results remain the caller's responsibility. Owner-side
calls and result/snapshot ownership remain serialized on the UI thread.

The worker test holds two actual original SQLite read transactions per Root
schema. It overwrites a queued caller scope/target array, checks the complete
copied result, refuses stale tickets, exhausts both slots, transfers once and
retains old snapshots across original read error. It also injects reply allocation
failure and retries. In the shutdown case it destroys an actual `nx_context` and
the app model before releasing the original SQL query; the late result is disposed,
the queued job is cancelled and all descriptors balance. This is UI-context/model
retirement, not native-window or AT-SPI retirement. Tests reuse `run_model.py`'s
private metadata fixture and require unchanged Root/User database bytes and rows
for all nine Root v2–v10 schemas. They create no real installed app payloads.

## Selected-app window

The trusted owner host accepts explicit configuration, never a GUI-selected source
path or an administrative request from an ordinary client:

```sh
./build-software/software_native FONT ROOT_DIR ROOT_EPOCH ACCOUNT UID [INSTALL_ID ...] \
  [--manifest INSTALL_ID MANIFEST_PATH ...]
```

Identities are 32 hexadecimal characters. The complete selected set is copied into
the owner/worker; zero targets means no selected targets, not an empty installed
inventory. The native host uses the normal accessible Wayland-only SDK profile
with AT-SPI support and a working private/session bus. It draws with the software
renderer. The source path and recipient still require the original owner's actual
admission; this sample must not be exposed as an arbitrary-path privileged service.

Filter/Read/Close are the list toolbar. Tab and Shift-Tab move through available
controls; Up/Down/Home/End reach the complete selected result, including unmounted
rows. Enter or a row's Select action opens read-only details. Back/Escape returns
to that full installation ID. PageUp/PageDown scroll the details or list. Details
include exact source epoch/boot/runtime, recipient/UID, install/component/publisher/
scope/generation/artifact IDs and original revisions/gate state. Unavailable rows
explain the ambiguity of absent/inactive/ineligible targets instead of claiming
removal or denial. Successful gate metadata is explicitly no launch ticket,
permission or running-process observation.

Filter opens an editable draft. Apply/Enter searches component, observation label
and full ID, with a 128-byte UTF-8 limit and no line breaks/NUL. Empty shows all
selected targets. Back/Escape preserves the draft without applying it. Clipboard
and IME use the existing field adapters; Apply waits for composition to finish.
Read alone schedules another original observation. Filtering, details and resize
do not dispatch any administrative work or scan. Selection remains a complete
installation identity, independent of filtering.

Each new observation or mode/filter replacement retires the previous semantic
context before releasing it. Drafts and full-ID selection survive; late actions
queued on the replaced context are discarded. Measured heights re-anchor the
focused full ID after refresh instead of relying on estimated row positions.
Undersized windows expose Close and retain the inspection state for restoration.
Fonts, owner and bound window outlive the view. The host unpublishes/destroys the
view, closes renderer/window, then stops/joins/drains the worker and releases fonts.
The worker itself never accesses those UI objects. Native close during held SQL,
pending foreign clipboard and resize/monitor-migration stress remain separate
software-composition checks; prior Recovery evidence does not silently cover them.

`SWG_GERMAN`, `SWG_LARGE`, `SWG_CONTRAST`, `SWG_LIGHT` and `SWG_SMALL` choose
example styles; large text is 32 logical pixels versus 16 normally. Motion is
reduced and all themes use opaque existing tokens. Font tests cover 800×600 dark
at 1×, 360×300 German/large/high contrast at 2×, and 480×360 light at 1.25×.
Long labels wrap and detail/help content scrolls; 64×64 and 240×96 keep Close
reachable. The declared 100,000-target synthetic view mounts fewer than 200
semantic nodes and tests full-ID navigation/filtering and allocation rollback;
it is neither real app enumeration nor an accepted memory/latency budget.

## Independent native check

`atspi_client.c` links only OS libatspi/GLib helpers. Build it with
`-DSWG_BUILD_ATSPI_CLIENT=ON` where complete development pkg-config metadata exists.
The recorded private carrier instead uses real headers and versioned libraries
with an explicit compiler command because its full pkg-config dependency chain is
unavailable. The command and input hashes are retained with the native results.

```sh
python3 examples/34_aetheri_software/run_native.py \
  --source /path/to/immutable-original-aetheri \
  --model /path/to/software_model_test --host /path/to/software_native \
  --client /path/to/software_atspi --font /path/to/font.ttf \
  --server /path/to/test_compositor_server --registry /path/to/at-spi2-registryd \
  --work-dir /private/fresh/native-run --german --large --small --contrast --ime
```

This creates private metadata fixtures/bus/socket, exercises ten OS-peer checkpoints
and compares exact Root/User database bytes/rows before/after. Original compositor
virtual keys, IME preedit/commit and independent AT-SPI reads check focus, full text,
old-reference retirement, retained filter draft and selected installation. The
host exits by its real focused Close button, then all owned children are reaped.
No physical keyboard/monitor, installed production IME, spoken screenreader or
current production Aetheri service acceptance is inferred.

## Exact manifest/dependency join

Each optional `--manifest INSTALL_ID MANIFEST_PATH` follows all selected IDs and
is explicit trusted owner configuration. Sources/paths are copied before worker
startup. No path is requested by the GUI or derived from a component name. The
worker first obtains the original app observation; only then can it open an
explicit source for an eligible selected install. Ineligible/unknown/unselected
targets do not cause manifest reads. Hash expectation comes from that original
row's exact artifact ID. The shared example31 document loader uses the original
`ae_fixture_verify_manifest`; no second decoder or hash routine is introduced.
It reads at most 131,072 bytes plus one overflow byte through a regular-file,
no-follow/nonblocking open and owns immutable bytes until its document is freed.
No declared file path, action or dependency is opened/executed.

Hash agreement alone is insufficient: the original manifest component and full
publisher identity must also match the observed row. The batch copies the entire
source/recipient/install/generation/gate/revision binding. A lookup against another
UID, source context, row or revision exposes no document, even if artifact bytes
still match. An original app-read failure retains the old observation/metadata;
manifest failure remains separately unavailable and cannot fabricate an empty
inventory or replace the app's observation with a successful prefix.

The detail text shows original declared version/target, file/action counts and all
zero-to-eight complete 32-byte dependency IDs. A missing source is unknown metadata;
failed verification or identity mismatch is unavailable metadata. Zero declared
dependencies is shown only for an available matching verified manifest. Installed/
active dependency state and ownership remain explicitly unknown. Publisher trust,
payload verification, current process/resources and management admission remain
outside this inspection window. Contents opens the matching manifest's complete
declared file/action tree. File details retain the full path, octal mode, byte
size and SHA-256; action details retain the exact executable, argument count,
each literal argument and its UTF-8 byte length, including empty arguments.
These are read-only declarations, with no action execution or payload access.
Dependency/ownership graph composition still requires actual owner observations.

Up/Down/Home/End navigate all tree rows. Left/Right collapse/expand groups; Left
from a leaf returns to its group. Enter, pointer activation and the accessible
Select action open the same declaration details. PageUp/PageDown read long paths
and details without changing focus. Back/Escape returns from a declaration to
that exact entry, then to Contents, then to the full installation identity.
Groups include their declared counts. Missing/unavailable matching metadata
disables Contents; it never displays a successful empty tree. Observation or
manifest replacement retires the old context and returns nested inspection to
installation details, so old file/action references cannot select a replacement.
Ordinal entry IDs are local to that exact manifest/context, not global file IDs.

`software_components_test` uses original-encoder maximum declarations (32 paths,
eight actions with eight arguments, eight dependencies), owned-buffer lifetime,
empty dependency declarations, wrong hash/component/publisher and exact observation
binding checks. It overwrites copied worker configuration, proves only the two
eligible configured sources are read, disposes messages/documents after transfer,
checks allocation/FD errors and reads the final dependency through large-text
details. Root v2/v10 bytes/rows remain unchanged after fixture preparation.

Append `--components match` to the native test command to bind a real original
observation to the original-encoder fixture and check all eight IDs through the
separate OS peer. `--components identity-failure` selects a canonical manifest
whose hash matches the original artifact row but whose component disagrees; the
native peer requires unavailable metadata without a fabricated zero dependency
count. These remain private synthetic metadata cases, not installed payload or
production-service adoption. The original encoder/vector/spec inputs are pinned
by `manifest-inputs.json` in addition to example33's 67 original inputs.
Append `--contents` with `--components match` to exercise all 32 paths/eight
action names, keyboard selection, full read-only argument/file metadata, long
detail scrolling and native reference retirement through the independent peer.
The private fixture includes a declared 16 MiB file with no payload, a 256-byte
path and eight 256-byte UTF-8 arguments; it does not create those payloads.

Long metadata details cache the existing measured text wrapper per original
paragraph and paint only intersecting paragraphs. The complete original UTF-8
value remains one read-only semantic Text snapshot without a caret. This avoids
repeatedly shaping a whole multi-line document while emergency-wrapping a later
long ID/version. Width, observation, metadata and state changes clear paragraph layouts.
Raster density updates use the existing context API and preserve logical layouts. The bounded original fields fit the 8,192-byte body and 256-paragraph cache;
this is an example-specific layout bound, not a general document/performance
budget. Native reader tests account for AT-SPI's outward-rounded integer extents
when counting fractional PageDown steps, and still verify the actual end bounds
and exact return to the top.

## Historical process observations

`software_history.h` adds an owner-side read adapter for explicitly supplied
original `ae_execution_profile` values, using only
`ae_catalogue_get_execution_process`. The existing worker and native History
view compose this adapter; no full inventory or live process observer is added.
Do not call it from a GUI frame. The trusted owner retains the
serialized original Root handle and admits the source, account and profiles.

`swh_read` copies at most 64 complete profiles, current scope and selected row
anchors before any original getter. It reuses the model's validation and original
profile codecs/hashes. Foreign epoch/account/boot, malformed profiles and duplicate
launch IDs reject the entire input before any original read. An unselected or
ineligible target causes no process query. Before/after source contexts must
equal the selected-app observation, including its revision; this is no cross-query
transaction, transport authentication or mutation fence.

Each result retains the original profile/generation/runtime and full recorded
starter/app values. Missing records and unsupported schema versions have explicit
per-role negative errno and zero snapshots. Unexpected I/O/allocation/corruption/
context errors reject the entire batch, preserving caller output and any earlier
batch. No successful prefix becomes a complete observation. `swh_get` returns a
borrowed range only for the exact current source/recipient/row/revisions. Batch
storage outlives that range. A retained snapshot can inspect it after the original
catalogue and owner close. No selected profiles means no selected profiles, not
no processes. Each unavailable role remains unknown, not a zero PID/process count.

These are **historical enrollment records**. A retained PID/pidfs/cgroup/unit or
past readiness label proves no current process, image, CPU/RAM metric, ownership,
permission, drain or replay right. Same-boot owner replacement can retain readable
history without adopting a run. A selected current generation can differ from
the historical profile; keep both exact identities visible. Current authenticated
ProcessFD/system-bus/supervisor/image evidence and complete discovery remain
external owner/service obligations. The adapter never opens a numeric PID, calls
a launcher, changes a catalogue/runtime/gate or performs a process/service effect.

The additional original fixture inputs are pinned by `history-inputs.json` at the
same immutable Aetheri revision. `software_history_fixture` is Aetheri's unchanged
metadata-only `test_launch.c`, compiled without native service/root-unit options.
`software_history_original` reuses its frozen process/profile vector and canonical
reference inputs to prepare private histories, including a completed/released
run, owner replacement and an explicitly corrupted witness digest. Original
migrations prepare Root v4–v10; Root v2/v3 stay unsupported. Read-only cases compare
database bytes/rows afterward. Tests exercise whole-batch failures, copied caller
inputs, exact row/UID/revision binding, full 64-bit IDs, the 64-profile limit,
malformed final profiles, unavailable roles and retained storage/FD lifetimes.
No synthetic witness is asserted to be an actual admitted kernel process.

## Recorded unit associations

Each selected historical record also retains the original profile-bound
`ae_catalogue_get_unit` and `ae_catalogue_get_unit_custody` observations. The same
serialized worker uses its checked current Root epoch/boot/runtime; original
getters bind the complete historical profile, including after an owner replacement.
Unselected/ineligible targets cause no extra query. Root versions before9/10 and
absent observations remain explicit per-field errors with zero snapshots.
Unexpected errors or malformed returned bounds reject the complete historical
batch; independently valid selected-app data remains usable. No private SQL,
unit/RPC effect, FD recovery, runtime mutation or service connection is added.

From a historical profile, **Unit links** opens a read-only page containing the
allocation ID, recipe/placement/selection digests and timestamps; custody origin,
bus/issuer/manager/label; node count and every recorded effect, node index,
intention, acknowledgement flag and timestamp. Escape/Back returns focus to Unit
links, then the profile tree. Paging, below-minimum restoration and metadata
retirement reuse the existing contracts. Allocation witnessing and any number of
RPC acknowledgements establish no current custody, independent retirement,
process/holder drain, lease release or right to repeat an effect.

The original custody codec permits non-UTF-8 manager-label bytes. The batch keeps
them intact. The page uses UTF-8 when valid and otherwise shows the complete
bytes as explicitly labelled hex, avoiding a text-layout failure or replacement.
The maximum test combines19 effects,255-byte unique bus names and511 non-UTF-8
label bytes; its origin strings are explicitly synthetic stress inputs.

`software_history_links` reuses unchanged original metadata fixtures for witnessed
Root9 allocation, Root10 custody (including all19 effects), same-epoch owner
replacement and unavailable older schemas. It checks original database byte/row
equality, full data/focus/scroll/retirement, malformed outputs, whole-batch errors
and worker-thread getters. `run_history_native.py --unit-links` inspects the same
page through the independent OS peer; its added `unit`, `unit-replacement`,
`custody`, `custody-max` and `custody-replacement` modes prepare only private
original metadata. They do not run actual bus/unit/process effects. Further
lease, operation/diagnostic/reason and actual current owner observations remain
separate composition work.

## Reservation and original lifetime receipts

The same bounded history batch now also copies `ae_catalogue_get_receipt` and,
when admitted, the three original `ae_catalogue_get_run_receipt` results. Reads
run on the existing serialized Root worker; the GUI does no original query.
From a historical profile, **Receipts** opens the read-only original metadata.
The page shows current query Root and original profile runtime separately, the
reservation/dispatch fields, optional invocation/cause, and every field/digest of
the three original lifetime receipts. The first preceding digest is labelled as
the original start result; later digests reference the previous receipt. The
counterpart quiet record remains a Root-local historical copy.

Reservation lookup uses the selected account/UID, checked current Root
epoch/boot/runtime and the explicit profile's broker epoch. Its returned
`system_runtime` is the **current query Root**, whereas the copied execution
profile retains its **original Root runtime**. These query fields authenticate
no broker/process. Lease lookup is insufficient: the copied reservation also
matches the profile's request fingerprint, broker runtime, operation and launch.
A different request is explicitly unavailable (`-EXDEV`) with zero output;
malformed source/lease, flags, optional values, canonical unit or bounds reject
the whole batch. A withdrawn receipt and recorded cause remain historical
metadata, never a present stop, holder-drain or authorization result.

Lifetime reads require retained original starter evidence and the original Root
runtime. Missing evidence yields `-ENODATA`; replacement yields `-ESTALE` without
using recovery APIs or reconstructing an invocation. Root-local closing,
counterpart quiet and release receipts retain all original fields and canonical
digests. The original codec/matcher validates the exact profile/invocation; each
later receipt must reference the preceding digest. A missing stage stays
unavailable (`-ENOENT`), and a later stage without its predecessor rejects the
batch. No foreign User catalogue, recovered lifetime capability or current
process evidence is acquired. Missing snapshots and hashes are zeroed.

`software_history_receipts` prepares unchanged original completed, withdrawn,
replacement, unsupported, unit/custody and corrupted metadata fixtures. It checks
database bytes and rows, source/request/chain faults, error propagation after
successful prefixes, explicit missing values, worker-only getters, independently
valid app replies and detach-once owned lifetime. Earlier snapshot/64-profile,
copying, row/revision and GUI regressions remain in the full example suite.
Escape/Back returns focus to Receipts in the profile details. Page keys reach the
whole text at the supported sizes; below-minimum restoration, metadata replacement
and owner-snapshot replacement reuse the existing retirement contracts. Missing
data is explicitly unknown and zero snapshots are never rendered as real IDs.
`software_history_links` checks all fields, unavailable/replaced/withdrawn cases,
three font/theme/density styles and normal focus/retirement. The independent
`run_history_native.py --receipts` peer reads the complete published text, verifies
fixture metadata, keyboard entry, read-only behavior, paging and retired nodes.
These are Linux virtual-compositor/OS transport checks, with actual installed IME,
spoken screenreader and physical display acceptance remaining separate.
Further operation/diagnostic/reason relationships, actual current observations
and the remaining catalogue gates remain open.
