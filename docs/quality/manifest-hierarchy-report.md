# Original Aetheri manifest hierarchy — 7 November 2026

Consumer/test source:
[`6a8bb795c42323ff42af69464619a9aa898baf30`](https://github.com/Lulus792/NexaUI/commit/6a8bb795c42323ff42af69464619a9aa898baf30).
Implementation `d42c562c09bae2144d33543fb202bd4c6b4b69a1`, native over-height
reading `7e06dcce83fdc34ca59c05fe794167256312e43e`; the final change only prevents
test imports writing bytecode into the original snapshot. All final C binaries
match the recorded native inputs; all three source tests were rerun afterward.
Installed SDK source remains `ac0fb3b3b3aa53b2a4de927ec52a06deb33d5c36`.
Original Aetheri source is `1fb45c7c21f914a2893457132b609be50ef4280d`.

This advances scoped L02/L06/L11/I01/I07/A02/A06/A10/D07/P04/P05/Q02/Q04 evidence
in delivery packet 6. All 103 requirement IDs and the remaining app scope stay
intact. It adds no public SDK API/ABI, SDK version or required SDK dependency.

## Existing contracts and actual data

[Example 31](../../examples/31_aetheri_manifest/README.md) compiles the original
`aetheri_core` and calls `ae_fixture_verify_manifest`. The existing original
decoder checks canonical AETHFIX1 and its SHA-256 against the caller's exact
expected ArtifactId. No replacement wire format, model, hash implementation or
catalogue is introduced. Original OpenSSL 3.5.9–3.5.x and Linux checks remain in
force. OpenSSL belongs to this optional Aetheri consumer, not NexaUI.

The normal executable accepts `FONT MANIFEST EXPECTED_SHA256`, creates its own
wakeup pipe and completes real reads on two workers. The independent native
automatic case selects the final complete path without any release gate. A
separate explicit test invocation supplies two FDs: the workers still read,
verify and project actual manifest bytes, then hold the already copied result
until released. The driver chooses neither result status nor row contents.

The source verifier pins 20 original core/build/contract inputs; configuration
rejects a deliberately changed original-core copy. Those original inputs still
match after the checks. Aetheri's live checkout, launcher, settings, default
presentation and 0.1.18 consumer pin are unchanged.

This view only exposes declared file-path metadata. It never opens a declared
payload path, verifies payload files, resolves/executes actions, opens an
authority catalogue, installs anything or confers publisher/permission trust.
Expected-ID provenance remains caller-owned. The original manifest requires
1–32 files, so this adapter cannot produce a successful empty manifest; the
neutral example retains its separately tested empty state.

## Identity, bounds and presentation

The original schema admits 32 strictly sorted, unique, ASCII paths of up to 256
bytes. The example response now holds every one of them, including terminating
NULs, instead of truncating to its previous 16/95-character sample bound.
This remains an example bound, not a limit on NexaUI's tree API.

Within one exact expected artifact, canonical path ordinals are stable row keys.
A changed file cannot publish a different list under those keys because original
digest admission fails. Changing the expected artifact requires retiring the
context/view and using a new epoch; this example does not implement an artifact
switcher or claim general positional identity across revisions.

Workers receive copied view/generation/branch keys and immutable source/digest
inputs. They open only the selected regular manifest, with nonblocking/no-follow
admission, read at most the original maximum plus one overflow byte, copy admitted
paths and free the decoder's borrowed input. Failure leaves caller output intact.
FIFO, symlink, directory, absent file, oversize, malformed, wrong digest, input
lifetime and repeated FD-balance checks execute in the C consumer.

Example 30's existing controls, semantic publication, S02 channel, poll-FD wakeup,
software renderer and focus handling are reused. The existing Settings emergency
grapheme-wrap helper prevents horizontal overflow; semantic names retain original
bytes. Measured label heights determine row bounds. Page Up/Down reads an
over-height focused row; subsequent frames retain the scroll rather than forcing
its top back into view. Fully fitting rows still require full native viewport
containment. Over-height rows instead require full width, initial top alignment,
actual keyboard traversal to the bottom and unchanged focus. Core real-font
checks cover width/height and scroll retention.

## Executed checks

Environment: Fedora 44 guest, Linux 6.19.10-300.fc44 x86-64, GCC 16.2.1,
CMake 4.3.0, Python 3.14.3, Wayland 1.26.0, xkbcommon 1.13.1, OpenSSL 3.5.9.
The reused software SDK profiles are Wayland-only, AT-SPI enabled, strict C17
with global `-Werror`. The sanitizer profile instruments both this consumer and
the original Aetheri core with ASan/UBSan, with leak detection enabled.

| Profile | Manifest CTests | Neutral hierarchy CTest | Native result |
| --- | ---: | ---: | --- |
| Static Release | 3/3, no skips | 1/1, no skips | 11/11 manifest stages; automatic input and light theme; 18/18 neutral stages |
| Shared Release | 3/3, no skips | 1/1, no skips | 11/11 German small/high-contrast manifest stages; 18/18 neutral stages |
| Static Debug ASan/UBSan/LSan | 3/3, no skips | 1/1, no skips | 11/11 default and German 200% small/high-contrast manifest stages; 18/18 German 200% neutral stages |

The independent libatspi peer has no NexaUI linkage. It checks all 32 exact path
names, Busy/failure/Retry, old Retry retirement, last-row focus/selection/readable
geometry, collapse during pending work, newer success followed by ignored older
failure, selected identity preservation and old-context/action retirement.
German small cases read both ends of the complete 256-byte path through actual
Page Down input, including 200% text. Four top/bottom capture conversions were
visually inspected at 100%/200%; no whole-app design or human reading acceptance
is inferred from them.

Each controlled positive host closes normally with
`submitted=5 posted=5 adopted=3 ignored=1 drained=1 joined=2 failed=0`.
The final worker posts its held result after `GUI_GONE`; the channel remains
alive until both workers join and remaining results are disposed. Automatic
cases close with `submitted=1 posted=1 adopted=1 ignored=0 drained=0`.
All accepted hosts/peers exit 0 without sanitizer diagnostics. Actual pending
filesystem I/O latency is not bounded by the post-decode release-gate test.

Three private counterexamples retain failures: bypassing original digest
verification fails the portable test and native stage 1; allowing stale
publication fails the portable test and native stage 8; fixed-height child rows
fail the real-font width/height test. The source guard separately rejects changed
original code. No deadlines were raised or diagnostics suppressed.

Earlier failures are retained: the test initially used a nonexistent span member;
the automatic driver's capture guard treated a normally finished peer as a
failure; ordinary Unicode word wrapping allowed unbreakable paths to overflow;
the small native test then exposed the need to read an over-height row rather
than claim full containment. The final checks retain full-containment requirements
for fitting rows and explicitly verify both edges for taller ones.

## Frozen evidence and remaining gates

Local bundle: `D:/NexaUI/out/nexaui-manifest-hierarchy-20261107.tar.gz`.
SHA-256: `ae6dcea6512d055ba4b0e3bdbaf4200fed9d288d7efee92bbf9ad0943b71aaf2`.
Size: 48,211,640 bytes; 2,939 regular files and 50 symbolic links, plus the archive
manifest. It contains original/consumer source inputs, build and command records,
installed artifacts, positive/negative cases, raw captures and initial failures.
Named pipes used by tests are recreated by the seed script. The archive and each
listed file/link were independently verified on the local host after transfer.
After verification, 69 loose capture duplicates (969,754,773 bytes) were retired
from only this step's explicitly listed owned case directories. Their complete
raw bytes remain in both verified local/guest bundles; no older frozen bundle
was changed. The exact-path/hash record is
`D:/NexaUI/out/manifest-capture-retirement-20261107.json`. Restore from the archive
before rerunning a collector that expects those loose PPM files.

Source proof checks 1,047 committed files, 20 original Aetheri inputs and 159 SDK
production/build/data inputs. The SDK comparison differs only in root CTest
registration; SDK implementation/artifact hashes are unchanged. Exact binary,
source, font and compositor hashes are in the bundle. Guest time is earlier;
only newly copied owned source mtimes were normalized, with no clock changes.

Original Files scans remain synchronous. General multi-branch/paged hierarchy,
artifact switching, actual catalogue/service/inventory integration, complete
pickers/software/device/trusted-system workflows, normal Aetheri owner adoption,
unbounded filesystem failure/latency, below-minimum fallback, further languages,
fonts, performance budgets and physical/human acceptance remain open. Existing
paused physical/interactive tests remain paused. This is an independently usable
read-only manifest consumer, not full component-browser adoption or a 1.0 release.
