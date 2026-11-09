# Original Editor: partial bytes and real clipboard EOF

Test source `9477f62539cc73b6746b776bca19c843cc623ea8`, 9 November 2026.
Installed SDK0.1.29 remains `8862cb5a1b78c63845a2d2a50441cda8e19243e7`;
compiled original owner/reader bundle40 remains unchanged. The original53 inputs
are from `7bbe7042ecd48582b11ef7053a4dbeade6b02351`.
This extends the [original clipboard composition](editor-clipboard-report.md)
with joined partial-byte/held-EOF evidence. No SDK/API, production application,
renderer or compositor implementation changes in this step.

## Independent transfer scheduling

The opt-in `--clipboard-stream-gate` argument adds a private test adapter only
to foreground foreign `wl-copy` children. The pinned executable remains
SHA256 `75f5428d5a36c0c9d06611d4020f580a4622400e2f64579ecb45644dc888a921`.
Its existing selection/transfer implementation supplies the protocol behavior;
this is an unmodified foreign binary under an explicit transfer adapter, not an
uninstrumented-provider claim. The provider's send callback uses `fwrite` then
`fclose`; the exact synthetic-payload/FIFO match intercepts that boundary.
See the [foreign implementation](https://raw.githubusercontent.com/bugaevc/wl-clipboard/v2.2.1/src/types/copy-action.c).

The standalone C17 helper links libc/libdl, with no NexaUI or original-model
linkage and no installation target. It must match the provider's musl ABI.
The copied child environment scopes the preload; app, compositor and independent
libatspi reader receive no injection. The Python wrapper retains existing
foreign tool/runtime hash checks and checks the actual provider mapping.
Build and invocation are in the [consumer instructions](../../examples/36_aetheri_editor_consumer/README.md#joined-partial-byte-and-eof-task).

A private inherited SOCK_SEQPACKET channel arms one later matching transfer.
A normal independent `wl-paste` first verifies the offered bytes. The adapter
then writes the first3 bytes of `é🌍\r\nlate`: complete `é` and an incomplete
UTF-8 globe prefix. It reports the actual write and retains the original pipe.
The next command writes the remaining9 bytes while still withholding EOF.
Writer-side `FIONREAD` queries record actual queued bytes; waiting for zero
proves consumption without opening another reader. These queue samples are
meaningful in stage5 replies only; other stage packets retain a previous value.
The final command returns to the original callback, whose actual `fclose`
closes the writer. Stage3 reports after that real close, not a modeled EOF.

## Newly executed native cases

The same private Fedora44/Linux6.19.10 producer, installed SDK13 prefixes,
owner40 and independent reader40 run with final frozen driver44. Original
rendering, capture sizes, 8-MiB document operations, IME, history/save/review,
style/density, cache checks and native close/move/resize/maximize tasks remain.

| Installed profile | New stream cases | Forced frames before EOF | Complete captures | Actual raw cache probes |
| --- | ---: | ---: | ---: | ---: |
| Release static | 7 | 26 | 84 | 83 |
| Release shared | 7 | 26 | 84 | 83 |
| Debug ASan/UBSan | 7 | 26 | 84 | 83 |

Every case forces two original pointer-event frames after the incomplete prefix
has been consumed. Six cases additionally force two frames after all12 bytes
have been consumed while the writer remains open. Text/caret/document revision
remain unchanged and the independent native request remains pending.

The complete case applies exactly once after actual EOF and checks original
Undo/Redo. Truncated EOF rejects the incomplete UTF-8 payload. Caret movement,
fresh preedit, preedit followed by cancellation, review/Cancel and tab roundtrip
retire the pending request after consumed bytes; late EOF cannot revive it.
This totals21 cases and78 forced pre-EOF frames, including the original model
and system clipboard rather than only memory callbacks.

The native close task separately consumes3 ASCII bytes and retains the writer.
The original Save and process exit0 finish while the same provider PID/pipe
remain open. This gated branch does not claim a SIGSTOP state. The same final
Shared driver without the gate also passes its prior ordinary native task,
78 complete captures and stopped-provider close check. Enabled tasks retain
the original78 captures each and add six; all four runs provide330 captures.
The existing1.2-second retained-close sample remains zero CPU ticks/zero frames;
this is a bounded lifecycle guard, not a power or performance benchmark.

The app and installed SDK remain instrumented in the native ASan/UBSan profile,
with leak detection and prior deadlines unchanged. The foreign provider uses
the separately hashed Release musl gate in all three profiles; it is not
claimed instrumented by the app's sanitizer configuration.

## Utility, negative and source evidence

Two separate local musl builds, Release and Debug ASan/UBSan, execute one CTest
each. Each uses real pipes for ordinary, held, truncated and reader-closed/EPIPE
cases: **2 new CTests,8 pipe cases,4 compiled objects**. Both gate and writer are
instrumented in the sanitizer utility build; the compiler's matching ASan runtime
is preloaded first only in that standalone writer child. Leak detection remains
enabled. Source, flags, binary/objects, dynamic tables and raw CTest logs are
retained in the [utility receipt](editor-stream-utility.json).

The final native test detects the actual archived pre-fix owner36 binary: after
consuming partial/all bytes without EOF and a fresh preedit, its missing serial
fence permits an old native paste. The independent `!accepted` assertion fails
as intended. The controller records that expected nonzero result; production
SDKs and installed prefixes are not replaced with defective code.

The [source binding](editor-stream-source-binding.json) binds102 frozen inputs
to Git:53 original and49 repository files. Relative to compiled bundle40, only
README and two uncompiled Python files change; six additions are test-only.
All96 owner/reader objects are reused. The previous45 owner model tests and41
compatibility tests remain historical/reused evidence, **not newly run CTests**.
Five generators are replayed and21 resulting native C files match byte-for-byte.
Four utility source/object associations match the frozen inputs, and the built
Release gate and actual foreign-provider gate are byte-identical.

Fifteen complete previously reviewed PPMs match their earlier original clipboard
captures byte-for-byte. Two new Shared captures are losslessly exported and
visually inspected: original layout, CRLF line placement and caret remain
visible. The borrowed mono font still shows a replacement box for U+1F30D;
glyph/fallback acceptance remains open. Independent assertions verify actual
UTF-8 document bytes and history. All330 complete captures are also decoded and
checked for dimensions, length and raw hash by the final evidence checker.

The [raw native receipt](editor-stream-local.json),
[checks and exact commands](editor-stream-checks.json),
[archive/retirement ledger](editor-stream-archive.json) and
[exact-source CI receipt](editor-stream-ci.json) preserve attribution.
Generic SDK/platform/package CI does not compile or execute the standalone
foreign utility or exported original app; their evidence is the local matrix.

All seven required jobs pass on exact source9477f62 in
[run42576515850](https://github.com/Lulus792/NexaUI/actions/runs/42576515850):
Windows144/143, macOS120/119, Linux265/263 and sanitizer265 in each discovery
order, with no CTest skips. The Linux static job also passes its separate
one-test repetition. Installed, close-request and add_subdirectory consumers
pass in all six platform jobs. Optional Windows-ASan/fuzz jobs skip; their
absence is not converted into new coverage. Seven complete logs and full REST
head-SHA/job metadata are retained and checked by the CI binder.

## Remaining acceptance

All53 consumed original inputs remain unchanged at Aetheri
`e01dad1184b292aa269d61f48d8baea0ee53ecef`. No active Aetheri source, pin, build or
default changes. Verified archived old captures/compatibility builds allowed
108,197,064 bytes of scoped inactive test output to be retired; installed SDKs,
frozen inputs and reused owner/reader builds remain. No quota, mount, driver,
deadline, sanitizer or rendering settings changed.

This closes the controlled joined partial-byte/EOF gap for the optional original
Editor clipboard composition. Native document geometry/attributes, broader
interfaces, AX/UIA document tasks, real installed IMEs/screenreaders, font,
physical/human/design/performance and normal Aetheri adoption remain open.
All103 catalogue IDs and remaining packet6/packets7–8 persist; no1.0 release.
