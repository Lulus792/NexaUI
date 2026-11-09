# Application document clipboard: SDK and isolated native consumers

SDK0.1.29 development, source `8862cb5a1b78c63845a2d2a50441cda8e19243e7`.
Independent native fixture `5203f67298b7e2d53b9c69d93166fc6a1a078589`.
Original Editor harness compatibility `2bac6ea08cfe0c3f43edddcb7966c4adbefc1bf2`.
9 November2026. Catalogue SHA256
`9c932872caeba8007bf05b86f7c167b493260709b23c54d709222a5f19539e65`;
all103 requirements and the complete eight-packet goal remain active.

## Reused contracts and new opt-in

The existing document registration deliberately refused clipboard requests.
`nx_text_document_register_with_clipboard` now accepts borrowed clipboard
callbacks and optional insertion validation. It reuses `nx_text_clipboard`,
`nx_text_clipboard_for_window`, the existing native request/completion queues
and bounded asynchronous Wayland reads. It creates no second editing model,
clipboard service or platform dependency. Existing registration calls retain
their replace/select-only behavior; existing public structure layouts remain
unchanged. See the [public contract](../concepts/accessibility.md#application-owned-document-text).

COPY, CUT and PASTE are explicit capabilities. Copy writes a copied snapshot
range, without calling the model or reporting a model edit. Cut writes before
one atomic deletion; paste validates a complete bounded insertion before one
atomic replacement. A later rejection preserves the document but cannot revoke
clipboard bytes already transferred. Read-only COPY requires explicit opt-in;
read-only cut/paste remain refused. Protected document publication remains
unsupported. Native clipboard payloads retain64KiB, document snapshots8MiB,
bulk cache4MiB and outgoing queue8MiB limits. Native clients retain their1000ms
call deadlines; the owned Linux native request retains its2000ms lifetime.

The application supplies a never-reused logical input revision for text,
selection and policy, including validator/clipboard reconfiguration. Model
callbacks must revalidate the original complete input token. No model,
clipboard callback or clipboard-user pointer is retained across frames.
A pending read retains the existing owned native token; every registration
supplies fresh callbacks and checks target/revision/policy, ancestry, native
lease and active region. Context destruction and omitted targets cancel it.
Native clipboard windows must outlive pending work and the context.

Protocol reference: GNOME's versioned
[AT-SPI2.60.7 EditableText declaration](https://raw.githubusercontent.com/GNOME/at-spi2-core/2.60.7/xml/EditableText.xml).
CopyText has a void wire reply. The reader checks its actual GError; libatspi's
TRUE convenience return alone cannot establish successful copy.

## Actual installed profile matrix

Fedora44, Linux6.19.10-300.fc44.x86_64, GCC16.2.1(Red Hat16.2.1-2),
CMake4.3.0 and Python3.14.3. Builds use C17/-Werror and the existing Wayland-only
profile, without X11/Vulkan. The debug profile instruments the whole candidate
library and compiled consumers with ASan/UBSan, retaining leak detection.
System libraries and the unchanged private test compositor are separate inputs.

| Profile | Focused CTests | Wayland paste scenarios | Installed package | Independent AT-SPI tasks |
| --- | ---: | ---: | ---: | ---: |
| Release static | 18 | 10 | 1 | 3 |
| Release shared | 18 | 10 | 1 | 3 |
| Debug ASan/UBSan | 18 | 10 | 1 | 3 |

The54 CTests include30 completed Wayland document-paste scenarios. The real
protocol fixture sends partial bytes from its server thread and holds EOF;
the document remains unchanged over multiple completed frames. Single-line
normalization and multiline preservation apply exactly once after EOF.
Rejection, revision change, capability removal/return, omission, native-lease
revocation, observed OS focus loss, explicitly expired native-lease fault state
and context destruction prevent application. The last expiry case is a guard
fault model, not a timing-performance benchmark.

Additional component cases check Unicode copy/cut/paste, model-free read-only
copy, callback failure, invalid UTF-8/NUL, oversized/mismatched reads,
validator rejection/revocation, reserve failure and lease revocation during
reserve. Failed consumed reads are not replayed on a later registration.

The9 independent AT-SPI tasks comprise the new clipboard reader, the existing
document reader and the genuine metadata-budget reader in each profile.
The new clipboard host intentionally supplies **in-memory callbacks**. Its
separate OS client checks scalar Unicode copy/cut/paste, read-only copy,
truthful native failure, the complete8-MiB document bound, restoration through
cut/paste and clean close. That fixture does not prove actual system-clipboard
transfer. Real delayed Wayland transfer is the separate protocol test above;
joining native AT-SPI, external clipboard peers and an original app owner
remains a required followup. Existing document/cache tasks pass unchanged.

## Compiled regression and provenance

An actual compiled pre-release candidate12 fails the accepted delayed paste:
taking EOF consumes its native token, and the following frame check rejects
otherwise current work. The identical ten-scenario test passes against
candidate13 in all three profiles. Completion now remains eligible in the
frame where its owned read was observed; target/revision/ancestry/native
guards still apply, and end_frame never retains a consumed token. The failed
candidate, source, object/binary files and full logs remain archived.

The [local matrix](document-clipboard-local.json) records54 CTests,3 installed
package consumers,9 native tasks,252 SDK objects and12 host/OS-reader objects.
The [source binding](document-clipboard-source-binding.json) matches1069 SDK
inputs to8862cb5 and9 native-fixture inputs to5203f67. No builder declaration
substitutes for those exact Git/byte bindings. The SDK tree is the first code
revision; the later fixture does not retroactively change its compiled inputs.

The [check ledger](document-clipboard-checks.json) and
[archive ledger](document-clipboard-archive.json) preserve the regression and
verified archive-only retirements needed for the native test UID's storage
budget. Frozen source exports and installed prefixes remain available. Only
inactive artifacts with matching archived type/mode/hash are retired; no
quota, mount, driver, deadline, sanitizer or rendering policy is weakened.

All seven required exact-source SDK jobs pass; the
[SDK CI ledger](document-clipboard-sdk-ci.json) binds seven complete decoded
logs, all six platform builds/consumers and both sanitizer driver orders.
The native-fixture [CI ledger](document-clipboard-native-ci.json) also records
all seven required jobs passing. Core/native-source Linux static totals are
264/265, shared262/263, and the respective sanitizer orders264/264 and265/265;
each static build also passes its separate lifecycle repeat. Windows144/143
and macOS120/119 pass for both revisions. New delayed document paste and native
clipboard tests execute in their relevant Linux/sanitizer jobs without CTest
skips; installed package/close/add_subdirectory consumers pass on six platform
builds per revision. Two optional jobs skip in each run.

## Existing original Editor compatibility

The controlled Editor driver previously allowed only0.1.27/0.1.28 receipt,
library and native-document versions. Source2bac6ea adds explicit0.1.29 entries
while preserving library hashes, source checks and other refusals. The first
bundle33 driver still refuses the third guard before starting the app; bundle34
changes that one uncompiled Python file and reuses identical compiled owner33
and reader33 inputs. This harness change has its own actual local task below;
the two SDK/platform CI ledgers above remain attributed to their exact sources.

The existing optional document adapter, with the new clipboard opt-in disabled,
passes15 original-owner CTests and the full Shared native task against installed
SDK13/0.1.29:32 owner/reader objects,13 raw cache queries and72 complete captures.
All7 generated C inputs replay exactly. Five current complete raw captures equal
the previously reviewed Unicode/review/large/contrast/light pixels. The final
unchanged1.2-second idle guard observes zero CPU ticks and zero new frames.
This is one Shared compatibility profile, not a new three-profile original
clipboard-adoption matrix. Its94 inputs are separately Git-bound. At the
freshness check, independently moving Aetheri HEAD
`dba762a630feca3cf14f27d36dd093a8a1a400ec` still has all53 consumed original
inputs unchanged. Initial driver/collector/archive refusals remain recorded.

## Remaining scope

The original Editor's existing optional document adapter still uses the old
registration API. Its keyboard clipboard remains app-owned; this change does
not claim native clipboard adoption in that app. Next, compose the new opt-in
with its original controller/token, history/save/review, IME, rendering and
independent system clipboard peers, including delayed owner wakeup and close.
The active Aetheri source, pin, defaults, services and build remain unchanged.

Native document geometry/attributes, broader text/cache behavior, new document
AX/UIA tasks, installed real IMEs/screenreaders, physical/human/design/
performance gates and ordinary Aetheri adoption remain open. Remaining packet6,
packets7–8 and all103 requirements are not complete. Version1.0 still requires
the user's explicit approval.
