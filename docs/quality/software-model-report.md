# Selected original software observations — 2026-11-07

Continued by the [bounded worker evidence](software-worker-report.md) at
`a7a920e`; the tests and scope recorded below remain this model-only checkpoint.

Consumer source: `64c9ab101bdaea2ff47e650ea911e81f1115935e`.
Original Aetheri: `46e2a21ef979d9a76db2a8487d1de167a6a79ef7`.
Selected installed SDK input: `c4f467d85b0147944d2456f5e7602a39ac18e579`, 0.1.23.
Scoped requirements: D07/L05/L11/P04/P05/P07/Q02/Q08.

[Example34](../../examples/34_aetheri_software/README.md) supplies an original
Root read producer and immutable owner-thread presentation model for a selected
set of app installation IDs. It is the data foundation for the software view,
not a native GUI or complete software/device management implementation. No SDK
implementation/API/ABI/version, live Aetheri source or consumer pin changes.
The [system-composition assessment](system-compositions-assessment.md) maps the
remaining device/software/component/trusted obligations to actual source seams.

## Source and identity behavior

The caller supplies trusted Root source identity/context, recipient account/UID
and full installation IDs. `sw_read` copies those inputs and uses the original
`ae_catalogue_get_info`/`ae_catalogue_resolve_app` APIs. It canonicalizes selected
IDs and rejects duplicates/oversize inputs as a whole. Original source contexts
and revisions must agree before/after and across successful rows. The producer
never queries private SQL, starts/migrates a runtime, caches a user binding,
opens a gate or dispatches an operation. Actual I/O belongs on the owner worker;
worker scheduling is the next increment.

The original API observes active eligible account/machine **apps** with committed
generation/verified artifact and separate gate metadata. A per-target `-ENOENT`
remains an unavailable selected row, with zero successful-install fields. It is
not relabeled as removed, uninstalled or explicitly denied. Other original errors
discard the entire in-progress batch rather than publishing a successful prefix.
Empty success means no selected targets, never a confirmed empty global inventory.
Context/revision checks establish neither a cross-query transaction nor authority.

The owner copies source/recipient/target identity once. Only its current strictly
increasing ticket can transfer a validated result into a new reference-counted
snapshot. Old tickets, actor/runtime/boot drift, recipient/target substitution,
revision rollback, changed values under equal revisions and malformed rows are
refused without consuming the input or pending ticket. OOM permits an exact
retry. An original negative error completes as unavailable while retaining the
last snapshot/selection. Read-only inspection selects a complete installation ID;
neither a row position nor gate-open metadata authorizes launch. Retained snapshots
outlive newer observations and owner destruction.

## Executed model and original API checks

Fedora 44, Linux 6.19.10, GCC 16.2.1, CMake 4.3.0, Python 3.14.3,
OpenSSL 3.5.9 and SQLite 3.51.2. Consumer sources compile as strict C17 with
`-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror`. Existing example33's
67-input original-source check is reused unchanged.

| Build configuration | CTests | Read-only original Root schemas | Explicit original runtime replacement |
| --- | ---: | ---: | ---: |
| Release / static SDK input | 2/2 | v2–v10, 9/9 | 1/1 |
| Release / shared SDK input | 2/2 | v2–v10, 9/9 | 1/1 |
| Debug ASan/UBSan/LSan / static SDK input | 2/2 | v2–v10, 9/9 | 1/1 |

The two tests are original API/model checks and the separate declared maximum
model. This package/header/build matrix is **not** native NexaUI rendering,
platform/shared-library-use or UI acceptance. No new SDK test run is claimed.
Sanitizers instrument the consumer and original libraries, with leak detection
and halt-on-error enabled and no final diagnostics.

Each private original Root fixture has two eligible selected apps (account and
machine), one other-account target, one preparing target and one unknown ID.
Five selected IDs yield two original observations and three `-ENOENT` rows.
All nine read-only schema cases preserve the exact Root/User database bytes and
table rows. One separately labeled original `begin_runtime` mutation rejects
the old source scope and permits a new observation under the new context. It is
not counted as a read-only case. These are real original API/SQLite calls with
synthetic eligibility metadata, not actual installed payloads or authenticated
production-service adoption.

Controlled failures cover partial-query errors, before/after revision/runtime
drift, wrong recipient/UID/source, duplicate/oversize targets, allocation failure
and retry, superseded completions, exact target binding even under a newer
revision, same-revision contradictory values, and retained snapshots. A source
wrapper changes the borrowed caller inputs during original queries; the copied
request remains bound to its original account/targets.

The separate declared 100,000-target model checks reverse input order, complete
ID lookup/selection and rejection of an invalid final row beyond the visible
working set. Its row array uses 36,000,000 bytes and each target array uses
1,600,000 bytes on this carrier. This excludes other arrays, contexts, allocator
peak and process/backend/GPU memory. No 100,000-real-install I/O or accepted
performance budget is inferred.

Four copied model defects build and fail intended assertions: accepting a wrong
ticket, omitting selected-target binding, accepting different contents at an equal
revision, and ignoring the post-query context/revision check. The first fixture
attempt failed an original UNIQUE(component,scope) constraint before reaching
the original read checks; the preparing fixture now has its own component.
Only that test-data error was corrected. Earlier sources/builds/failure remain.

## Source and artifacts

[Local receipt](software-model-local.json),
[archive receipt](software-model-archive.json),
[read-only API survey](software-api-survey.json).
Eight final source inputs match the exact consumer commit after only CRLF-to-LF
normalization. All seven executed inputs remain unchanged; README was added after
execution. The later README clarification says duplicates are rejected, not
silently removed. Current Aetheri `497822f` interface inspection is separate from
the exercised older immutable source. Its unchanged projection declaration alone
does not prove current runtime/schema/service compatibility.

Prior main `fbe3ce4` has all seven required CI jobs successful, with two optional
skips; [exact-run API receipt](recovery-clipboard-ci.json). These optional consumer
tests are local evidence, separate from root CI. The archive collector's own live
log and terminal journal entry are outside the frozen boundary. Earlier artifacts
and Aetheri's independent active changes are retained.

`D:/NexaUI/out/nexaui-software-model-20261107.tar.gz` contains 13,960,559 bytes;
SHA-256 `a740281c8e4fd20a0016fc875514f861bca6c0b9443b9fc4db52e38941444d51`.
Independent local verification checks 1,308 file hashes/sizes/modes, two link
targets and 616 directory modes. Source inputs, all build/test/negative outputs,
private original fixtures, SDK package inputs, the API survey and command journal
are included. The source revision identifies the exercised model/test code;
subsequent documentation clarifications do not change those executed inputs.

## Remaining scope

Copied bounded worker jobs/channel/wakeup and close/lifetime behavior, the visible
software list/filter/details, native keyboard/IME/clipboard/AT-SPI, images and
exact manifest/dependency joins are next. Full installed inventory, device/service
control, current process/resources, permissions, operation/removal/trusted host
composition and installer/recovery admission still need their actual owner seams.
No device backend, complete inventory, process readiness or administrative effect
is invented from these selected observations. The rest of packet 6, packets 7–8,
declared performance budgets and physical/human acceptance remain open.
