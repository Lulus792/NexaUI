# Original-owner Recovery worker and native view — 2026-11-07

Consumer: `9d09ec0e49f3cc2f8b1a6d6e836528f964c96146`.
Installed SDK: `c4f467d85b0147944d2456f5e7602a39ac18e579`, version 0.1.23.
Original Aetheri: `46e2a21ef979d9a76db2a8487d1de167a6a79ef7`; all 67 pinned
build/core/schema/fixture inputs match. The live Aetheri source and pin are
unchanged. Requirements touched: D11/L04/L11/I01/I07/T04/T11/A02/A06/P04/P05/Q02/Q04.

## Delivered composition

[Example 33](../../examples/33_aetheri_recovery/README.md) now has an ordinary
Wayland software host and a private in-process worker using existing public
S02, wake-FD, input, IME, semantics and presentation APIs. Original catalogue
open/get-info/read/close calls stay on one worker thread. The UI receives an
owned immutable original inventory and a copied session/request key, and checks
that key and its ticket before adopting it. Two channel reservations bound
accepted work and its completions. No copied SQL or recovery action is added.

Stop rejects new requests, queued reads complete as cancelled, and a read already
running may finish. The host retires its native publication and GUI before
stop/join/drain. Startup synchronously waits for the worker's open/get-info
handshake; inventory scans are asynchronous. Neither handshake nor join promises
a filesystem timeout. This entirely trusted process is not an IPC/security
boundary, a normal installed owner service or a complete Recovery app.

The view disables Apply for invalid input and active native preedit; Enter
cannot apply the committed prefix during composition. A settling frame updates
same-frame validation. Native raster density is preserved for replacement
contexts. A pointer resize grip returns a host intent using the existing public
resize API; actual native resize/output migration remains unverified here.

## Executed checks

The carrier/toolchain and SDK backing results are documented in the separate
[Wayland backing report](wayland-shm-backing-report.md). Strict C17 consumer builds
use `-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror`. Release static,
Release shared and Debug ASan/UBSan each pass **3/3 CTests**: original observation
model, worker and real-font view. The model/view checks retain their earlier
owner/schema, synthetic maximum-size, selection, filter, layout and allocation
scope described in the [component report](recovery-view-report.md).

The new worker check uses real private Root v10/User v8 owners and holds the
original SQL read inside an active SQLite transaction while destroying a GUI
context. It verifies worker-thread ownership, bounded admission, cancellation,
join/close ordering, wake readiness, draining two completions after one pipe
drain, stale-result ownership, once-only transfer, retained snapshots after read
errors, reply-allocation failure/retry, wrong-source refusal and final descriptor
balance. Original database bytes and rows remain identical. GUI context teardown
in this check is not native window retirement during a held transaction.

Four final ordinary native workflows pass with normal host and independent
libatspi peer exits, ten checkpoints each, unchanged owner bytes/rows and no
forced cleanup:

| Build | Owner and configuration | Full-detail pages down/up |
| --- | --- | ---: |
| Release static | Root, English, ordinary size/dark | 0/0; entire text fits |
| Release static | Root, German, 32px text, 360×300, high contrast, native IME | 12/12 |
| Release shared | User, English, ordinary size/light, native IME | 0/0; entire text fits |
| Debug ASan/UBSan | Root, German, 32px text, 360×300, high contrast, native IME | 12/12 |

The frozen Aetheri compositor supplies virtual outputs and native Wayland key
and text-input-v3 events. The separate OS client links no NexaUI code or headers;
its common transport helpers are reused from example 30. It reads full Text,
checks read-only caret/selection/edit refusal, retired row/detail/filter
references and refused old actions, End selection, detail PageDown/PageUp and
Back focus, original Read refresh, literal typing, preedit-disabled Apply,
committed accented text, local empty/full filtering, selection restoration and
keyboard Close. AT-SPI actions are actual OS requests. This is not a human
screenreader or physical display review, and no native Tab/Shift-Tab or filter
Cancel acceptance is inferred from the component tests.

Two isolated consumer defects compile successfully and fail intended checks:
suppressed worker wake fails poll readiness; enabled Apply during preedit fails
the independent AT-SPI enabled-state assertion at checkpoint six. The normal
IME workflows pass. A separate SDK EDQUOT defect also fails its intended check.
No test deadline, detector or assertion was weakened to obtain these results.

All **22** final exported consumer/common/peer files match the consumer revision
after only CRLF normalization. Actual hashes remain in the
[machine receipt](recovery-native-local.json). The independently loaded shared
library and all four final host/peer/font/compositor/registry identities are
retained. ASan/UBSan covers the selected SDK, consumer and original catalogue;
the OS peer and external system libraries are not all instrumented.

## Retained failures and remaining gates

The [archive receipt](recovery-native-archive.json) identifies the frozen local
`D:/NexaUI/out/nexaui-recovery-native-20261107.tar.gz`: 73,866,740 bytes, SHA-256
`01d5e712d1811417f2fd7e9c58201505d5f1fa4eaf1015e2891ed932199d02d7`.
Independent local verification checks every one of its 3,448 files, four link
targets and 1,978 directory modes, plus sizes/hashes/file modes. Both nested
build checkpoints are separately fully verified (input3: 326 files/212
directories; input1: 96 files). The archive includes original source, tested
source snapshots, installed SDKs, binaries, fixtures, commands and raw results.
The live collector's own log/result are outside the frozen test boundary.

The archive retains input1/input2 builds, quota/space failures, partial peer
upload/SyntaxError, input3 and final input4 sources, both SDK configurations,
all attempted native workflows and all counterexamples. The first native host
terminates with SIGBUS on old sparse `/tmp` backing. A separate native attempt
on a build without optional D-Bus headers fails publication. These are failures,
not successful native acceptance. Sanitizer input2 fails resource/fixture setup;
only the later fully built input4 profile supplies the final sanitizer result.

Still required: native held-transaction completion wake and close before gate
release; native Tab/Shift-Tab/filter Cancel; resize, compact restore and output
migration; clipboard peers; native retained references after whole window close;
real authority/service transport, reconnect/unknown outcomes and actual recovery
admission/readiness. Original owner observations do not prove global readiness,
external-holder drain, device/software inventories, installer/first-run or trusted
login/lock integration. Those packet 6 workflows and packets 7–8 remain in scope.
Physical/human accessibility, hardware, graphics, lifecycle and measured
performance gates remain separate. No Aetheri consumer adoption or 1.0 release
is claimed.
