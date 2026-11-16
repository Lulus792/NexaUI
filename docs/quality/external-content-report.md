# External CPU frame handoff

Standalone consumer `5fa854d9b0f68c7951ef418801fb846accd248bb` starts the
packet7 content seam using existing installed NexaUI 0.1.33/source
`368cd38292498bd538964ba514ff02e0e9924358`. It adds no installed SDK API,
engine dependency, Aetheri implementation or application-pin change.
[Consumer and build/run instructions](../../examples/38_external_content/README.md)

## Existing contracts and the missing consumer boundary

`nx_snapshot_exchange` transfers immutable producer-owned snapshots between a
producer and UI consumer; its borrow must end before the UI frame ends.
`nx_viewport` instead leases image pixels through rendering after `nx_end_frame`.
Directly retaining the snapshot borrow or returning its later-freed pixels
cannot satisfy both contracts. Extending the snapshot borrow would change its
public lifetime rule. This consumer instead copies each new accepted frame into
an explicitly bounded staging buffer and releases the snapshot inside acquire.
The ordinary viewport lease then protects staging through rendering.

The app supplies a fresh content generation after session/tab replacement.
Viewport resize records an increasing request ID and requested physical size.
The producer echoes that tuple and a monotonic frame sequence in an owned
in-process header/pixel allocation. It uses existing snapshot publication and
disposal, not another thread/channel implementation. The consumer validates
generation, resize, sequence, dimensions, RGBA8 format, stride, payload length
and budget before copying. Equal sequences reuse the same immutable staged
pixels; lower sequences and mismatched requests are rejected.

Allocation/budget failure preserves the previous request and staging. Transition
to zero-size suspension retires staging and advances the request. Destroy while rendered
pixels are leased fails and leaves the object usable for completion. The app
retains the exchange and worker lifetime, so an in-flight job can finish after
the old surface and context close; the replacement generation rejects its frame.
The 64MiB maximum is this example's explicit accepted-payload/staging policy,
not a global SDK limit or a bound on arbitrary upstream engine allocations.

## Executed evidence

Clang22.1.3 on Alpine-Dev/WSL2, uid65534, passes **12 installed consumer CTests**:
four each in static Release, shared Release and Debug `-O1` ASan/UBSan/LSan.
C17/extensions OFF and conversion/shadow/Werror remain enabled. Sanitized
positives use leak detection and no suppressions. The shared executable resolves
the selected installed SDK; all three SDK library hashes match the prior
source-bound package receipt. No fonts or content engines are needed by these
headless compositions.

| Case | Observed behavior |
| --- | --- |
| Flow | Padded producer rows, unchanged-frame reuse, scale resize, suspension, old-request rejection and budget failure/recovery |
| Malformed | Truncated header/payload, wrong generation/request/dimensions/format, zero sequence/stride, excessive stride and decreasing frame sequence |
| Lifetime | Resize allocation failure preserves request, active-render destroy refuses, completion/close succeeds, replacement rejects the old generation |
| Worker | A real OS thread prepares an old request, waits through surface/context close and replacement, then publishes; old content is rejected and fresh content renders |

Rendered cases compare all 16,384 bytes of a 64x64 RGBA composition against a
separately built reference, including the ordinary host button and content
rectangle. Tests reacquire the exchange before end_frame to prove the snapshot
borrow already ended. They also publish another producer frame, disposing the
old allocation before rendering the prior command stream, to verify staging
remains readable. All tracked surface/viewport allocations balance at teardown.
Unchanged-frame tests observe no additional staging allocation/copy; these are
work counters, not performance or input-latency measurements.

Three private compiled counterexamples fail at the intended boundary:

- Removing the generation check admits an actual worker result for the closed
  tab and fails the expected stale-frame rejection.
- Retaining the snapshot borrow fails the independent second acquire before
  UI end_frame.
- Returning producer pixels after releasing their snapshot produces a real
  ASan heap-use-after-free when that allocation is retired before rendering.

The last diagnostic is a deliberately broken consumer, not an SDK regression.
The test source is unchanged in all negative copies. All configure/build steps
pass; each probe exits1 at its expected check, within the existing 15-second
test budget. No outer timeout is accepted as a diagnostic result.
[Source/object/command/test/negative receipt](external-content-local.json)
and [independent archive readback](external-content-final.json).

The first compiler pilot rejected a tautological 64-bit size comparison. The
corrected code checks viewport dimension limits and divides the explicit byte
budget before multiplication. A later build-helper preflight accidentally
renamed a hash function and failed before configure; corrected targeted naming,
both pilots and the intermediate strict profile results are retained. A worker
join review also prevents the unexecuted Windows test branch from freeing state
under a still-live worker on join failure. Windows/macOS consumer builds remain
unexecuted acceptance gates.

`nexaui-content264.tar.gz` contains 108 regular members, 8,618,463 bytes,
SHA-256 `a455aa25db854189a9a9d3d6164e908fee7b9dd534a3f13cf4fb6c65448c9b71`.
It retains the exact consumer/catalogue inputs, compiled objects/binaries,
complete logs, pilot preimages, negative source and helpers. Every member was
read back and hashed twice. The SDK implementation remains `368cd382`, whose
seven required [platform/package CI jobs](https://github.com/Lulus792/NexaUI/actions/runs/43760877115)
are separately source-bound. That SDK CI run does not build this new consumer.

## Remaining packet7 acceptance

This section records `5fa854d`. The
[native follow-up](external-content-native-report.md) adds ordinary-window,
worker-fd, host-focus and independent displayed-pixel evidence at `2ef9a56`.
Selected-engine, guest-input, IME and engine-accessibility gates remain open.

This advances the CPU image/lifetime boundary for R06/R10/D08/P05/Q02/Q08.
It does not establish a maintained browser/PDF/guest engine, native content
presentation, input/host escape, stuck-key release, IME/candidate geometry,
engine accessibility subtree, crash recovery, GPU import or physical/human/
performance acceptance. Those remain concrete integration work and named engine/
host gates. Physim and NexaCloud retain their separate real-app adoption gates.
The fixture does not decode documents or execute guest/service operations;
producer pixels never become host permission authority. The full 103-ID catalogue
and remaining packets stay active. Aetheri can independently adopt this bounded
CPU handoff where its own producer and lifecycle policy satisfy the contract.

## PNG producer checkpoint, 2026-11-17

Consumer `564e5e980e633b3a16ecbd3c957073746cdb2ccf` uses installed SDK
`b53513b390dbf135050a858fbdfd434edc04ad08` and Alpine libpng `1.6.59-r0`.
The optional producer uses the [versioned libpng read API](https://raw.githubusercontent.com/pnggroup/libpng/v1.6.59/png.h)
to decode borrowed PNG bytes into straight-alpha RGBA8. It fits that image to
the requested physical size with transparent margins, then transfers the
complete frame through the existing exchange and viewport. Unit coverage
includes alpha, centred fit and reduction pixels, truncated data, size limits
and reuse after rejection. A compiled variant that admits a 4096-pixel source
fails the source-dimension assertion.

The native fixture decodes four valid PNGs and rejects one truncated PNG
before publication. It exercises old-generation rejection, accessible error
status, retained image and semantic object, recovery, 2× backing and close.
In all three profiles the malformed input leaves every one of the 93,184
displayed image pixels intact. Recovery updates 5,824 pixels, with 23,296
bytes staged through the existing damage contract. The worker reports five
completions, four publications and four disposals on normal close.

On Alpine with Clang 22.1.3, 43 installed CTests and sixteen private native
cases passed. These include static Release, shared Release and ASan/UBSan/LSan
with PNG enabled, and a default build without libpng linkage. The standalone
host's dynamic link audit records `libpng16.so.16` only with the option on.
Archive `nexaui-content-png384.tar.gz` binds exact source, commands, fixture
PNGs, logs, captures and the counterexample. Readback verified SHA-256
`81c7ac570c5ab8ced3e8f1ed699efbf366b7f71945839f62a1c60c5dd7eb5cdf`.
Local receipt `content-png384-final.json` records the library/header and
member hashes.

Production image viewer navigation, process-level decoder isolation, broader
format and metadata handling, PDF/browser/guest engines and GPU handoff
remain separate acceptance work.
