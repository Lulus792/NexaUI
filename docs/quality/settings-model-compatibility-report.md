# Settings model compatibility and retained Sound state

Consumer `7d460242be65ea157fefe295a015def35c8aee15`, 9 November 2026.
Initial build correction `540fb7f6da309bbf15d4804bcc99274a3118d0aa`.
Current original model `e1cd481c3bc1351b923aad1d08f57431ee9753b3`;
legacy model `7bbe7042ecd48582b11ef7053a4dbeade6b02351`.
Installed SDK0.1.31 `9f0b2294d04d7f5fdd63d3fe5c118b2bd222eade`.
Catalogue SHA-256 `9c932872caeba8007bf05b86f7c167b493260709b23c54d709222a5f19539e65`.

## Behavior and ownership

[Example21](../../examples/21_aetheri_model/README.md) again builds with the
current original Aetheri Settings model. Its existing display/appearance/language
view now has the original `src` include root and compiles the original UTF-8
helper when present. Current Sound snapshot validation needs that helper and
`audio/types.h`. A minimal legacy model-only export remains supported without
any text/audio module. No application model or UTF-8 parser is replaced.

The optional `NEXAUI_MODEL_BUILD_SOUND_TEST`, default OFF, builds an additional
existing-model/view composition test. A pending original volume intent survives
view creation, actual queued keyboard changes to contrast/language and view
replacement with its exact original target/revision. The observed volume stays
unchanged. Only the original model's explicit take operation consumes the intent,
once. Its permanent owner-loss fence also survives another view replacement and
continues to reject requests and observations from the retired session.

Those are synthetic original-model observations and headless UI events, not
authenticated device observations or effects. This view still has no Sound page,
audio-service connection or intent dispatcher. Aetheri already owns its newer
Sound presentation and audio host; reusing those exact original implementations
is the next composition step. No GLib, PipeWire, new worker, timer or device
operation is added by this build correction or test.

## Executed checks and source binding

Alpine-Dev/WSL, Clang22.1.3, C17 with extensions OFF and
`-Wall -Wextra -Wpedantic -Werror`. Tests run as UID65534 on a private native
Linux filesystem. The sanitized SDK and consumer use ASan/UBSan, with leak
detection and halt-on-error enabled. Borrowed Arial bytes are not redistributed.

| Original model | Release static | Release shared | ASan/UBSan/LSan | Objects per profile |
| --- | ---: | ---: | ---: | ---: |
| Current, complete8-file export | 5/5 | 5/5 | 5/5 | 10 |
| Legacy, minimal3-file export | 3/3 | 3/3 | 3/3 | 7 |

Total: **24 CTests and51 actual objects**, no skips. The existing view/text-wrap
tests and unchanged original Settings tests run for both generations. Current
models additionally run the original Sound tests and new retained-state test.
The legacy builds compile no optional text implementation and do not build the
Sound-specific test. The earlier21-test matrix at540fb7f is retained separately
and is not added to these totals.
The final [bounded verification](settings-model-compatibility-bounded-tests.json)
reruns these same24 cases as UID65534 with an explicit60-second limit per test
and the same sanitizer policy. These are not24 additional distinct cases.

The [local/source ledger](settings-model-compatibility-local.json) binds all25
inputs to exact Git blobs:8 current original files,3 legacy original files and14
consumer/common files. Every actual object source matches those inputs, and
installed SDK library hashes match the earlier complete SDK receipt. Compared
with the initial consumer, only CMake/README change and one test is added;
existing view/host/wrapping implementation bytes remain unchanged. Production
SDK sources, public API/ABI/version and backend dependencies remain unchanged.

Read-only verification at Aetheri2a1f88124ff12f161c80beead627b275e29f3047
finds all8 current consumed files unchanged. No active Aetheri file, build,
branch, pin or default is modified.

## Failures and counterexamples

The original1f311a5 consumer actually fails to compile the current model at
`audio/types.h`, with configure0/build1. A separate540fb7f counterexample keeps
the corrected include root but omits only the UTF-8 compilation unit; its actual
link fails at `ae_utf8_next`. Both are preserved as build evidence rather than
reported as native/device failures.

The final retained-state test also runs against a separately compiled mutation
which resets the borrowed original model during view creation. It fails at the
queued Sound-state assertion, exit1, while the unmodified final implementation
passes. All5 actual control objects use ASan/UBSan and the unchanged final test;
LSan remains enabled with no sanitizer diagnostic. The mutation changes only the
view source, not the original model, test or installed SDK.

## Evidence, CI and remaining work

The [initial archive](settings-model-compatibility-initial-archive.json) is
`nexaui-settings-model2-20261109.tar.gz`,25,988,133 bytes,
SHA-256 `f3c298d8ea8ce0e66c429c9824a8b5776dbc31e0ef220d52851e52f6d8a85a01`,
361 verified files. It retains the original
[include-root failure](settings-model-compatibility-initial-failure.json) and
[missing-UTF8 link control](settings-model-compatibility-missing-helper.json).
The [final archive](settings-model-compatibility-archive.json) is
`nexaui-settings-model3-20261109.tar.gz`,40,422,407 bytes,
SHA-256 `55126c710644da2a6a827fe3b858182902586874eef89eda1b00e64b0e01cc37`,
444 verified files. It retains final inputs, complete harness/build/test logs,
objects/binaries and the reset counterexample. Borrowed font bytes are excluded.
The [bounded-run archive](settings-model-compatibility-bounded-archive.json)
records `nexaui-settings-model3-bounded-20261109.tar.gz`,4,646 bytes,
SHA-256 `8f4d523baf986809b0bdcf725de005716a37cb5c5882c5eef1889ef96a0570c4`,
11 verified files covering the final explicit timeout commands and logs.
SDK0.1.31 prerequisite tests retain their original source attribution and are
not new executions in this increment.

The [CI ledger](settings-model-compatibility-ci.json) for
[run42653996046](https://github.com/Lulus792/NexaUI/actions/runs/42653996046)
binds all seven successful exact-source SDK jobs: Windows static/shared146/145,
macOS122/121, Linux static268 plus one lifecycle test/shared266 and268 in
both sanitizer driver orders. The first macOS static job was cancelled because
GitHub could not acquire a hosted runner; no build ran in that job. A single-job
retry succeeds as129300821868. Successful aliases in the new
attempt have byte-identical logs and are not additional executions. Two optional
jobs are skipped. No tests, runner architecture,
deadlines or sanitizers were changed for this retry.
The [initial CI ledger](settings-model-compatibility-initial-ci.json) and
[initial CI archive](settings-model-compatibility-ci-archive.json) retain25 verified
files,450,799 bytes, SHA-256
`dd70715bcbdbfb8796437aeb47a7130b07928abac6a28ec71ec799fb883139f0`, including
the initial cancellation, accepted queued retry and complete reused logs.
The [final CI archive](settings-model-compatibility-final-ci-archive.json)
retains35 verified files,480,948 bytes, SHA-256
`c43f7eb6bf3b02bf0c17042a221301deee0ed65ad5eab384d21c2c0f9c4f04f0`,
including the complete successful final job/metadata and one rejected/corrected
receipt-path binding preflight. No source or CI rerun was needed for that helper
correction.

This standalone external-model matrix is not executed by ordinary SDK CI.
The production SDK tree is unchanged from1f311a5. This example-only development
checkpoint claims no new SDK runtime, native Settings/Sound integration or
complete quality acceptance.

Next reuse the original Sound view/host with actual admitted owner snapshots,
native keyboard/assistive actions and original lifecycle/wakeup. Device/service
admission and effects, complete Sound/native Settings integration and physical/
human/design/performance acceptance remain open. The separate earlier
[Editor held-EOF finding](editor-attributes-report.md#native-gap-and-next-evidence)
also remains unresolved. All103 requirement IDs,13 surface groups and remaining
packets6–8 persist. This is a development checkpoint, not a1.0 release.
