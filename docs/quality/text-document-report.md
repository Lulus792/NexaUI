# Application-owned accessible documents: SDK foundation

SDK **0.1.28**, source `f5090602def620ce5a822c4301d6e4fdfaadf696`,
9 November 2026. Catalogue SHA-256
`9c932872caeba8007bf05b86f7c167b493260709b23c54d709222a5f19539e65`.

## Behavior and application ownership

[`nx_text_document_register`](../concepts/accessibility.md#application-owned-document-text)
copies an application-owned document into the existing semantic tree and applies
at most one admitted edit/selection through an ephemeral owner-thread callback.
The SDK retains no application/model pointer or callback in a native snapshot.
It creates no second editing/history/file model and does not draw the document.
The application must revalidate its complete original token and perform one
atomic operation; failure preserves its model. The SDK reserves result storage
before the callback, then rechecks native lifetime, ancestry and current policy.
Successful callbacks return truthful text/selection and a newer revision.

The text bound is8 MiB; native replacement payloads remain64 KiB. Linux AT-SPI
Text replies are limited to1 MiB each, with explicit failure for an oversized
reply and complete range reads instead of silent truncation. A new document
does not duplicate its full value in the custom `nexaui-value` attribute/event.
REPLACE/SELECT permissions are explicit and separate from built-in model
identities, even when numeric identity/revision values coincide. A read-only
document can permit selection. Deferred native replies reflect actual model
application; Windows UIA retains its documented admission-based setter behavior.
Existing public structure layouts and older function signatures are unchanged.

This is the reusable SDK prerequisite for the previously tested
[original Editor model](editor-accessible-model-report.md). The native fixture
uses its own small application model; it is **not** the original Editor adapter.
Only the example's CMake version check now accepts compatible SDKs starting
at0.1.27, allowing subsequent0.1.28 integration. No active Aetheri source, pin,
default owner, service API or persistence is changed. Aetheri can continue
independently. No1.0 release is made.

For the next adapter, all53 original-model inputs were separately rechecked
read-only against Aetheri HEAD `91f35712de5fbf900cb7faf6a1ec6d560296d72e`
and remain unchanged. This preflight is not a new Editor build or integration.

## Executed final local checks

The frozen SDK7 export and installed native consumer are separately built on
Linux x86-64, kernel6.19.10-300.fc44, GCC16.2.1. SDK and native fixture objects
use C17, extensions off and `-Werror`. The ASan profile instruments the whole
SDK and both fixture executables, with UBSan halt and leak detection enabled.

| Profile | Relevant CTests | Installed package run | Native document task | SDK/test objects + native objects |
| --- | ---: | --- | --- | ---: |
| Release static | 13 passed | passed | passed | 73 +2 |
| Release shared | 13 passed | passed | passed | 73 +2 |
| Debug ASan/UBSan/LSan | 13 passed | passed | passed | 73 +2 |

The13 CTests cover the new document contract and existing semantics, text-value
ancestry/completion, read-only snapshots, general/numeric fields, paste, pointer
editing, text buffers/allocation failure and AT-SPI text diffs. The installed
package consumer calls the exported API and verifies Wayland-only build features.
The independent native peer links only OS AT-SPI/D-Bus/GLib libraries; its host
uses installed NexaUI through the public header. The reader's transitive native
dependencies are separate from the Wayland-only SDK dependencies.

Each native task uses a fresh private bus/registry and the frozen three-output
producer16. It checks Unicode scalar edits, combining text, selection/caret,
partial UTF-8 refusal, unsupported clipboard rejection, callback allocation
failure, pending stale edits, disabled/read-only state, all8 MiB through16 exact
range reads, oversized-reply refusal, full-document deletion, pending detach
refusal and clean host/compositor exit. No application screenshots or rendering
performance are claimed by this semantic-only fixture.

Two separately compiled and linked defects are detected: omitting the native
lifetime check after allocation, and dropping the built-in/application target
domain check. The second also fails an earlier policy-drift case in the full
test; a separate invocation of the existing domain oracle isolates the intended
identity-collision rejection. Earlier helper lookup/assertion failures are
retained and are not counted as successful mutation checks.

[Actual commands, flags and artifacts](text-document-local.json),
[checked totals and remaining work](text-document-checks.json),
[exact Git/source binding](text-document-source-binding.json).
All1055 frozen SDK inputs and six native-fixture inputs match the stated commit
byte-for-byte or by explicitly recorded line endings. There are225 retained
compiled SDK/test/fixture objects and400 selected artifact bindings. The linked
builder revision remains empty; provenance is established externally rather
than inserting an unverified revision into a binary. The producer executable
and all three installed SDK libraries were rehashed against their archives.

## Failures, resources and CI

Earlier constructor/test-fixture and extracted AT-SPI development-prefix build
failures remain archived. `libatspi`2.60.7's `CopyText` wrapper returns TRUE even
when its void D-Bus call reports an error; the final native test therefore
requires the actual error and unchanged document, following the
[versioned OS source](https://github.com/GNOME/at-spi2-core/blob/2.60.7/atspi/atspi-editabletext.c#L134).

Several native attempts stopped before producer READY with SIGBUS and no SDK
host/client started. Separate backing probes reported EDQUOT while the global
filesystem still reported free space. After exact archived inactive artifacts
were retired, a128-MiB reservation and unchanged native binaries succeeded.
These failed attempts provide no native API acceptance, and the exact faulting
instruction was not newly traced. Backing-failure handling remains open under
P04. No quota, mount, driver, global cache, timeout, CPU guard or sanitizer rule
was changed. Five separately checked retirements retain their backing archives;
SDK trial source copies3–5 were retired, while original Aetheri source exports,
producer16 and all installed SDK prefixes remain.

[Archive/member/hash and retirement receipts](text-document-archive.json)
retain final SDK7 objects/tests/packages, final native7 binaries/runs and
historical SDK3–6 failures. GitHub run
[42538509517](https://github.com/Lulus792/NexaUI/actions/runs/42538509517)
passes all seven required jobs at the exact source. Windows static/shared
execute143/142 tests, macOS119/118, Ubuntu261/259 plus a separate static
presentation repeat, and sanitizers261 tests in each of two driver orders.
The new independent document task executes in both Linux builds and both
sanitizer orders; no CTest is skipped. Both optional Windows-ASan/fuzz jobs are
skipped. Installed package, close-request and ABI consumers pass on all six
platform builds. [Complete checked job logs and steps](text-document-ci.json).
These existing native AX/UIA regressions do not replace independent tasks for
the newly added app-document entry on those platforms.

## Remaining work in the full catalogue

Connect this SDK seam to the original Editor controller/render state, then
exercise actual independent Editor assistive tasks alongside original history,
dirty/save/close, composition, delayed paste and native lifetime changes.
Document clipboard actions, geometry/attributes, broader native text interfaces
and notifications remain open. Actual installed IMEs, physical displays, human
screenreaders, measured performance/design and normal Aetheri adoption are
separate gates. This checkpoint leaves all103 IDs, remaining packet6 and
packets7–8 active; the earlier Software AT-SPI instability and resource-backing
findings are not declared fixed.
