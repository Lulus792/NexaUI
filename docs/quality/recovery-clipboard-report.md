# Native Recovery clipboard transfer and pending lifetime — 2026-11-07

OS peer/driver source: `0ddad5ea1a09a480abfef7f1c44077bd2a82b162`.
Reused host source: `38210e00ea34fe9611ac01145f05a4828d852bf7`.
Installed SDK: `c4f467d85b0147944d2456f5e7602a39ac18e579`, 0.1.23.
Original Aetheri: `46e2a21ef979d9a76db2a8487d1de167a6a79ef7`.
Scoped requirements: D11/L04/I01/I05/T11/A02/A06/P05/Q02/Q04.

[Example 33](../../examples/33_aetheri_recovery/README.md) already binds its
filter to the public native text clipboard adapter. This step tests that
composition with independent wl-clipboard 2.2.1 tools and the existing separate
OS libatspi peer. No production host, view, worker/model, SDK implementation,
public API/ABI, version, live Aetheri source or dependency pin changes.

## Actual native workflows

All cases retain the preceding ten ordinary checkpoints, including original
read-only records/details, paging, stale references, local filter editing and
selection restoration. They then reopen the empty filter through native Enter.

1. An unchanged foreign `wl-copy` process owns the fixed UTF-8 value
   `Grüße 🌍 é`. Its selection publication and temporary popup destruction
   precede an independent `wl-paste` byte comparison. Native Ctrl-V pastes into
   the filter; the OS peer reads the exact full value and actual field focus.
2. Native Ctrl-A/C copies that value. The driver requires the actual native
   `set_selection` request and the prior foreign owner's cancellation/normal
   exit before `wl-paste` reads the exact bytes. A matching value alone cannot
   establish which process owns the selection.
3. A second foreign owner publishes a distinct value, then stops. The driver
   observes its actual `/proc` stopped state and the host's native receive
   request after Ctrl-V. The OS peer confirms the original draft is unchanged
   while the foreign producer has not sent the response.
4. In the **normal** case, native Tab leaves the field before the producer
   resumes. The parent observes a later source-send event and that child blocked
   again on its Wayland socket after the send callback returned. No additional
   clipboard probe steals the consumer's native focus during this barrier. A
   harmless native Shift key pair and subsequent frame precede the OS check:
   the unfocused field still contains the original draft. Escape returns to the
   full selected record; the retired field refuses late edits, then keyboard
   Close ends the host.
5. In the **close** case, Close is focused beside the pending field. Native Enter
   must unmap and normally terminate the host while the foreign source remains
   stopped. The independent peer requires the old field and Close references
   to reject edits/actions. Only cleanup resumes and terminates that source.

Each case completes exactly two original backend reads, with identical original
database bytes and rows. Clipboard, inspection and local filtering dispatch no
backend work. Clipboard text remains filter data and grants no authority.

## Executed matrix and counterexamples

Fedora 44, Linux 6.19.10, GCC 16.2.1, Python 3.14.3, SQLite 3.51.2;
the same frozen original compositor, real font and private OS registry/runtime
are reused. The independent peer compiles with real OS headers/libraries under
strict C17, `-Wall -Wextra -Wpedantic -Werror -O2`, without NexaUI headers or
linkage. Optional system pkg-config discovery is outside this direct-build scope.

| Installed SDK profile | Root normal / close | User normal / close |
| --- | ---: | ---: |
| Static Release | 2/2 | 2/2 |
| Shared Release | 2/2 | 2/2 |
| Static Debug ASan/UBSan/LSan | 2/2 | 2/2 |

The twelve final workflows pass: six normal cases with sixteen checkpoints and
six pending-close cases with fifteen. Root uses German32, 360×300, high contrast
and the preceding controlled IME checks; User uses English16, 800×600 and light
theme without that IME extension. This is not paste during active composition.
Four current-driver regressions also pass: native resize/German32/IME,
shared/User navigation, ordinary/User and ASan/User held-transaction close.

Two private defects compile and fail intended boundaries:

- Removing the consumer's clipboard adapter prevents the native paste at step11.
- Removing the SDK's paste focus-serial/focused-ID checks accepts the late reply
  after Tab and fails the independent unchanged-draft assertion at step14. The
  exact same shared host and OS-peer binaries pass with the original library;
  the failed process maps prove that the private mutant library was loaded.

These are test-only copied sources/libraries, never installed over a production
prefix. The private SDK build has an empty/unknown revision and an explicit
mutation receipt. Four used ordinary/held hosts and three installed SDK libraries
are hash-bound and copied/retained. No new core CTest or positive SDK test run is
claimed; prior [three-profile checks](recovery-resize-report.md) remain attributed
to that source. Final surviving instrumented hosts have no sanitizer diagnostics.

## Harness boundaries and retained failures

The first foreign stdin spool failed with EDQUOT before application paste. The
unchanged tool now receives fixed public test text through its documented
[text-argument path](https://github.com/bugaevc/wl-clipboard/blob/v2.2.1/src/wl-copy.c).
An initial probe also raced foreign popup publication and could read the same
old value before native Copy completed. The final synchronization verifies
publication/popup retirement and prior-owner cancellation, rather than retrying
the native action or inferring ownership from equal text. The pinned tool's
[copy-action implementation](https://github.com/bugaevc/wl-clipboard/blob/v2.2.1/src/types/copy-action.c)
provides the selection roundtrip and synchronous argument send used by these
process/protocol witnesses. Initial attempts and a rejected diagnostic revision
label/stale builder retry remain saved, with their actual failures.

The five private foreign tool/runtime files match the recorded prior binary
hashes. They are test dependencies, not NexaUI dependencies. Every clipboard
connection uses this run's private compositor; no account clipboard, package
installation or global quota/cache setting is changed. `/proc` read/poll witnesses
require a parent-owned Linux x86-64 child and access to its bounded descriptor
array; another carrier needs a separate witness.

## Source and evidence

[Local receipt](recovery-clipboard-local.json),
[frozen archive receipt](recovery-clipboard-archive.json).
All 24 source inputs match the exact commit after only CRLF-to-LF normalization;
only README changed after execution. The peer source originally compiled from
input2 is identical to the final source. The original 67-input check passes again.
The previous resize archive and all earlier artifacts remain unchanged. The
archive collector's own live log/terminal journal entry is outside its boundary.

`D:/NexaUI/out/nexaui-recovery-clipboard-20261107.tar.gz` contains 21,745,599 bytes;
SHA-256 `9fbec408e074b0904847a4ffdf1d3cdf3801772ee5185eebda4959a2c2aca10e`.
Independent local verification checks all 2,340 file hashes/sizes/modes, six link
targets and 421 directory modes. It includes source inputs, the exact reused
hosts, SDK prefixes, original fixtures, native logs/results, foreign tools,
private mutations/builds, failed attempts and the command journal.

Prior main `542210a` has all seven required CI jobs successful and two optional
skips; [exact-run API receipt](recovery-resize-ci.json). These optional native
consumer workflows are separate local evidence. Later CI is not inferred from
that prior result.

## Remaining scope

This establishes selected ordinary filter clipboard/late-focus/close behavior
with one independent clipboard implementation and a private virtual compositor.
Malformed/oversized values, active-composition clipboard/resize, other clipboard
managers/platforms, fractional/mixed-output movement and font glyph coverage
need their separately scoped checks. Exact text bytes are not visual or human
screenreader acceptance. No new visual acceptance is claimed for the unchanged
view. EOF still cannot prove an external sender delivered its complete value.

Production original-owner/service transport, reconnect/unknown outcomes, actual
recovery authorization/readiness and remaining device/software/component/
installer/trusted packet 6 compositions stay open, followed by packets 7–8 and
the rest of the 103-requirement catalogue, including declared performance and
physical/human quality gates.
