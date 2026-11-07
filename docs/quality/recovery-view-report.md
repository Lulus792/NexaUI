# Recovery list/filter/detail component — 2026-11-07

Consumer source: `88d12b4a3c226437d4ddc92d4390196cdac4ca25`.
Installed SDK: `ac0fb3b3b3aa53b2a4de927ec52a06deb33d5c36`, NexaUI 0.1.23.
Original Aetheri: `46e2a21ef979d9a76db2a8487d1de167a6a79ef7`.

## Delivered scope

[Example 33](../../examples/33_aetheri_recovery/README.md) now composes the
[original-owner observation model](recovery-observation-report.md) with existing
public virtual-list, search-filter, action, text-field, scroll, theme and read-only
Text APIs. The UI receives immutable copied metadata, not a catalogue/path or an
authority operation. It returns only Read and Close intents to its application.
There is no runnable native Recovery host or production transport in this step.

The list measures visible/overscan rows, navigates the whole filtered inventory
with Arrow/Home/End, and opens complete details. Tab/Shift-Tab reaches the toolbar;
Page keys read long details while Back retains focus. Back restores the inspected
row; Escape returns before closing. Local filtering searches kind/state/full ID,
preserves applied results on Cancel, and retains the draft. Invalid/overlong
queries cannot apply a prior prefix. Bound-window IME/preedit and clipboard hooks
are present; actual native acceptance is still required.

Details expose all original record fields and full owner/epoch/boot/runtime/
schema/catalogue-revision context through a read-only Text snapshot. They display
the observation/error state and the metadata-only readiness limit. Unread,
reading, observed-empty and unavailable remain distinct; errors retain the last
data and selection. Compact mode keeps Close available and restores prior mode,
identity and focus when enlarged. Ordinary shapes and colors use SDK themes with
reduced motion; there are no new effects or animation timers.

Full kind/16-byte keys govern inspection and selection. Row semantic numbers are
ordinals only inside an immutable observation. A successful snapshot or mode/
filter replacement creates a fresh context and retires any bound publication
first; old ordinals cannot be silently reused in the same context. Failed filter
or snapshot allocation preserves the prior context, inventory and applied filter.
Native held-provider/reference behavior and broader failure/rebind UX remain open.

No SDK implementation, public API/ABI, version or dependency changes. The optional
Linux consumer links its own standard math operations explicitly with `libm`.
The original-source check now also covers the imported `validate_contracts.py`
fixture module: 67 original inputs, versus 66 in the preceding observation step.
Old source/evidence keeps its original count. Aetheri source, live catalogues,
quarantined fixtures and consumer pins are unchanged.

## Executed component checks

Fedora 44, Linux 6.19.10, GCC 16.2.1, CMake 4.3.0, Python 3.14.3,
OpenSSL 3.5.9 and SQLite 3.51.2. Strict C17 flags retain
`-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror`.

| Installed profile | CTests | Original view owners / style cases | Declared maximum-size view cases |
| --- | ---: | ---: | ---: |
| Static Release | 2/2 | 2 × 3 | 2 |
| Shared Release | 2/2 | 2 × 3 | 2 |
| Static Debug ASan/UBSan/LSan | 2/2 | 2 × 3 | 2 |

The original model test also retains fourteen view-only owner/schema fixtures
and two explicit original backend runtime replacements per profile. Those runtime
mutations are separate from the view-only byte-equality claim. Instrumentation
covers the sanitizer SDK, consumer and original libraries; leak detection and
halt-on-error stay enabled, with no final sanitizer diagnostics.

The new component test uses real private Root v10/User v8 observations. It checks
public frame/input/action/semantic behavior with a real Liberation Sans font at
800×600/16px/dark/1×, 360×300/32px/German/high-contrast/2×, and
480×360/16px/light/1.25×. Complete original database bytes and table rows remain
unchanged. Checks include whole-list Home/End, read-only full details, geometric
end-of-content reading, toolbar Tab/Shift-Tab, filter draft/Cancel/Enter, invalid
queries, compact restoration, retained failed observations and Read/Close intents.

Two separately named synthetic 65,536-record inputs per profile exercise the
original result ownership shape, full-ID filtering, bounded mounted semantics
(under 200 nodes), selection after a shifted snapshot, and allocation-failure
rollback. They are in-memory presentation inputs, not actual maximum-sized owner
databases, complete device/software inventories or an RSS/latency benchmark.
Native offscreen assistive traversal and broad Unicode reason-content rendering
are not established by these cases.

Three independently compiled defects fail at intended assertions: suppressing
focus restoration, making End target the first record, and erasing the read-failure
semantic state. Controls use unchanged flags, bounds and deadlines.

## Headless visual evidence

The public headless renderer produced twelve successful Root-owner captures in
iteration 7. Six production component/model/wrapping inputs are byte-equivalent
to final iteration 9 after CRLF/LF normalization. Final tests extend Tab/Cancel,
maximum-size and allocation checks; they do not regenerate those images.
Five captures were inspected: ordinary dark/light lists and the narrow German
high-contrast list, filter and detail end. Long details end inside the viewport;
Back remains reachable. Large narrow toolbar labels wrap within their buttons.
These are component-rendered images, not actual Wayland presentation, installed
IME or human assistive/visual acceptance.

The nested `captures-input7.tar.gz` retains all 21 raw PPMs: twelve successful
images and nine from an earlier quota-failed attempt, including one partial
image. Its SHA-256 is
`214651455b003a0f08ad5846c8b9fd64e3d5f730577a4dc5baf95e4542534372`.
All archived file bytes/sizes/modes were independently verified locally before
loose duplicates were retired. Preliminary partial output is not counted as a
successful image.

## Artifacts and retained failures

[Machine receipt](recovery-view-local.json). Local accepted archive:
`D:/NexaUI/out/nexaui-recovery-view-20261107.tar.gz`.

- SHA-256: `677473036cb8d567f164275e749409f19d9555174ec241f57eaed0409caa4c37`
- 33,182,054 bytes; 1,394 regular files, 2 links and 1,180 directories, plus manifest.
- Fourteen tested inputs match the exact committed consumer/code archive.
- Original full source, 67 selected original inputs, three installed SDK prefixes,
  real font, all iterations/commands/logs/corpora/counterexamples and nested
  retired-output archives are preserved.
- After same-session encoded transfer, the whole archive was independently
  checked locally for every regular hash/size/mode, directory mode and link target.

Retained earlier failures include strict indentation/API-signature errors,
cross-source CMake cache reuse, a real focus bug caused by querying semantics
before end-frame, an incorrect mandatory-scroll assumption for fully visible
desktop details, missing direct shared `libm`, and reversed capture-render arguments.
The focus bug was fixed in the component; the complete desktop-detail geometry
now supplies the zero-scroll control instead of weakening content-read checks.

Quota failures interrupted shared image output, sanitizer compilation and a later
sanitizer link. Each failed attempt remains unaccepted; terminal processes were
resumed explicitly after precise duplicate retirement. Retired prior model files
remain in the verified previous archive; 348 older view build outputs remain in
the independently verified nested `preliminary-build-outputs.tar.gz`
(`bb74a9b508ca46828e083b0a0b085eb1cb85c989efa6c802c0cefa82ac595015`).
Receipts bind every retired file. A first private `/dev/shm` archive was unavailable
in the next SSH session, so transfer/verification stopped before deletion. A
same-session encoded stream was locally verified before loose-output retirement;
the accepted preliminary archive was then restored to the persistent private
`/tmp` workspace. No global quota, clock, cache, driver, test deadline, sanitizer
setting or production resource bound changed.

## Remaining adoption gates

D11/L04/L06/L11/I01/A01/A04/A10/P05/Q02 gain scoped component evidence. This does not
close native Recovery UI, actual IME/preedit/clipboard/AT-SPI clients, held native
references, window close, worker wakeup/join, production reconnect/unknown work,
complete device/software inventories or recovery/admission actions. The next
step is the native host/worker and independent OS-peer composition using this
component. Installer/trusted surfaces, the remainder of packet 6, packets 7–8,
final visual/performance and physical/human acceptance remain active across all
103 IDs. Aetheri can evaluate this presentation component independently; no
normal app migration or version 1.0.0 acceptance is implied.
