# Strict C17 SDK builds and native capability checks

7 November 2026. SDK implementation:
[`0d36408c120bd3ec59747688fa0ae6e94fc2dfe6`](https://github.com/Lulus792/NexaUI/commit/0d36408c120bd3ec59747688fa0ae6e94fc2dfe6).
Native management test:
[`8f90afac74571375604b8b330c59ee3103746f54`](https://github.com/Lulus792/NexaUI/commit/8f90afac74571375604b8b330c59ee3103746f54).
Base: `8169639d9d535d6f529fa88dd8269ec7bc73e48b`.

## Bounded production change

The previously failed global-Werror SDK build is reproduced on the exact base.
Four production files now remove those diagnostics without changing compiler
options, public headers, API/ABI, dependencies or development version 0.1.23:

- Rectangle/clip commands use named aggregate members; omitted fields retain
  their original C zero-initialization semantics. Independent plot statements
  are placed on separate lines.
- Grid comparisons explicitly convert the bounded loop value to the existing
  track enum. Track order, numeric operations and validation are retained.
- Optional line-break neighbours and the native/extra-FD/writable poll arrays
  start initialized. Source inspection found the existing success/count guards;
  no runtime uninitialized read was established. This is an explicit initialized
  state and compiler-diagnostic fix, not a claimed reproduction of such a read.

On Fedora 44/GCC 16.2.1/CMake 4.3.0, each installed SDK profile builds without
warnings using the same `-Wall -Wextra -Wpedantic -Werror` probe flags. All select
Wayland only, software rendering, no Vulkan/examples/root tests. The sanitizer
profile additionally enables ASan/UBSan, frame pointers, leak detection and
halt-on-error without suppressions.

| Strict SDK profile | AT-SPI | Library SHA-256 |
| --- | --- | --- |
| Static Release | ON | `8d38c79d7be9990d6a5cfde410fbbd8e86addd54be18c76b95463e3efb585a23` |
| Shared Release | ON | `7a8dfa35975bb47ec7886eae70ed0bbfa8c0e0b2bfcb5d06a199152d4f6a9e10` |
| Static Debug ASan/UBSan | ON | `852988482e0d1007c2bb1ba2ba143a34787cbfc66ac2a585ed97112b68b362fc` |
| Shared Release | OFF | `6a7700c9081f26ab50c0af07989c1d5063e3f507bdee25ebedb32267b7cdfe6b` |

These are checked Linux software profiles, not a warning-free claim for every
platform, renderer, example or test target. Regular root-test builds retain the
project's original flags; their assertions and deadlines are unchanged.

## Executed regressions and native scope

| Regular root profile | Passed | No-display skips |
| --- | ---: | ---: |
| Static Release | 144 / 150 | 6 |
| Shared Release | 143 / 149 | 6 |
| Static ASan/UBSan | 144 / 150 | 6 |

The static-only difference is `nexaui_selection_set`. Each profile passes all
19,346 bundled Unicode line-break samples, long combining/space sequences,
malformed-input preservation, grid/layout, drawing/clipping, paint and existing
headless reference/baseline checks.

The six display-dependent cases are then executed separately on the unchanged
pinned Aetheri compositor in each profile: window lifecycle, single-FD polling,
management, multi-window polling, allocator reentry and peer destruction. All
18 separate native executions pass. Reentry/peer destruction and the updated
management test link the installed strict SDK; other cases use their regular
test builds and existing private test hooks. This does not turn the original
JUnit skips into same-build native passes. The management source is pinned
separately at the test revision above.

Three installed original Files consumers also pass all three original CTests
each and their real document/save/close path. Aetheri source remains
`27de5cb228da0f37264fad64d964ad7a660d210d`. All 21 original independent peer
invocations pass, the same original document pair remains alive through dirty
close/cancel and Files waiting, exact `helloseed` bytes are saved, and hosts exit
normally without forced-kill escalation. These are the previously documented
[selected document checks](aetheri-adoption-assessment.md#original-files-document-save-and-close-lifetime-2026-11-07),
repeated against the new strict SDK artifacts. The external child remains the
original SDL Terminal; no consumer pin or application source is changed.

All 47 complete before/after PPM pairs match byte-for-byte: 45 native Files/
document output images and two public paint/icon images. The baseline runtime
uses the previously built SDK source `5118d3afd048a94430ca649fb345fb51f4f6965f`;
159 selected SDK source/build/data inputs are byte-identical to base `8169639`.
The original static baseline provenance is retained separately. No timing or
memory-performance improvement is inferred from these comparisons.

## Minimize capability finding

The old generic native management test fails on both the candidate and baseline
runtime because it unconditionally requires `NX_WINDOW_CAP_MINIMIZE`. This
Aetheri compositor explicitly omits that capability. The SDK already returns
`NX_ERROR_UNSUPPORTED` correctly; no capability or backend result is fabricated.

| Observed capability | Updated test |
| --- | --- |
| Offered | Keeps the original request, known-state and restore assertions. |
| Absent | Requires the exact unsupported error. |

The native runs exercise the absent-capability branch. An isolated shared-library
preload returning success for that unsupported request fails the new assertion.
The SDK library is not modified by this counterexample. Native positive
minimize/restore remains dependent on a compositor offering that capability;
default-platform CI is separate from these local target checks.

An initial native harness reached the ASan case before its root-test executable
had finished linking. That setup error is retained as an audit record with its
empty child log and server startup; it is not an SDK test execution or a pass.
The live root build was awaited and only the missing native cases were resumed.
No deadline, warning or sanitizer check was relaxed.

## Artifacts and remaining acceptance

`D:/NexaUI/out/nexaui-strict-sdk-20261107.tar.gz`, SHA-256:
`178de34bf444a1ddf1342b2b02e1b6df3000786f65093203fb523ba5a6b6fc6c`.
All 7,123 manifest files and ten symbolic links are independently verified after
local transfer. The archive contains 1,012 source-file matches to the SDK revision,
the separately verified management test, source archives, flags, installed SDKs,
regular test builds/JUnit/logs, native campaigns, original/baseline images and
negative sources. Large trees are streamed into the archive rather than copied.
Guest/host clock differences are explicit; only copied source mtimes were
normalized. Overwritten intermediate helper bytes are not claimed as retained.

The [base CI](https://github.com/Lulus792/NexaUI/actions/runs/42317132441) completed
all seven required jobs successfully. Local X11 headers are absent, so a new
default dual-backend or Windows/macOS strict build is not claimed here. Current
revision platform CI, other renderers, physical devices/outputs/resume, installed
IMEs and human accessibility remain separate gates. Real grants/authorities,
durable recovery, the unpaced Files sanitizer burst and held-pointer loss finding
are unchanged. This advances B05/W03/Q01 without closing those whole IDs or the
103-requirement catalogue. Aetheri's current pin and the 1.0.0 approval gate remain.
