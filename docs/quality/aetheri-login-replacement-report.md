# Native login view replacement with queued requests

Consumer `2c7e4ce60981d08393eec343163e2904e4c1a92e` exercises replacement of
an attached example37 view while original answer bytes remain queued. It keeps
installed NexaUI 0.1.33/source `368cd38292498bd538964ba514ff02e0e9924358`
and the unchanged eight Aetheri inputs from
`7d5fa4723c13460e9d61dc7db33b6a521409053f`. No SDK implementation, Aetheri
checkout, active pin or production service changes.

## Publication and ownership

The host creates the new view against the same original connection, constructs
its sending frame and publishes that new context through existing
`nx_window_publish_semantics`. Only after successful publication does it destroy
the old view. Native routing continues through the new context in the same
window; prior native handles and requests are invalidated by the existing
context-switch contract. The original connection, poller, descriptors, pending
bytes and absolute request deadline retain their owners and values.

The private `--connection-loop --replace-view` fixture holds transport while
the independent libatspi client observes the replacement's sending label. Its
borrowed control pipe wakes native polling and releases that observation gate.
This is a controlled pre-send checkpoint, not production transport scheduling
or a new timeout policy. The client checks retired fields and Answer objects,
invokes a retained old Action and requires rejection, checks that old labels do
not revive at the next replacement, then allows the original request to send.
The same secret/visible keyboard, Compose and cancellation workflows continue.
[Build/run and lifetime instructions](../../examples/37_aetheri_login/README.md)

## Executed evidence

Static Release, shared Release and Debug `-O1` ASan/UBSan/LSan pass five CTests
each; the default minimal-input build passes one: **16 consumer CTests**.
All builds use Clang22.1.3 on Alpine-Dev/WSL2, C17/extensions OFF and strict
warnings. The imported original musl mixed-sign exception remains isolated;
own sources keep conversion/shadow/Werror. Sanitized positives keep leak
detection and no suppressions. Installed SDK library hashes remain unchanged.

**45 private native tasks** pass: the preceding 36 free-model/inline-connection/
readiness/loss workflows plus **nine native replacement workflows**, one
keyboard, Compose and cancellation case in each profile. Every replacement case
observes two replacements before their respective exact response sends. The
host verifies the peer is empty before replacement and before release, receives
exactly two response frames and checks no leftover peer bytes at close. The
independent client observes fresh visible input/session controls, stale objects
and rejected prior actions, then closes with Escape. The 35-second client,
60-second Weston and 100-frame bounds remain; replacement construction frames
also count toward that frame bound. Loss regressions retain the original
EOF/admission/deadline uncertainty checks.

The initial pilot wrongly selected a same-named window as the sending label.
That window intentionally survives replacement. Its second-checkpoint failure
is retained; the corrected selector requires the label role. Three corrected
pilot workflows and the final nine replacement workflows pass. No existing
ordinary assertion or deadline was relaxed.

Two private compiled variants fail with the same independent native client:

- Binding the replacement to no original connection fails the first sending
  checkpoint, rather than silently starting another conversation.
- Omitting old-view destruction fails visible IME preedit observation. The same
  binary in a separate keyboard workflow reaches native close, where the
  child-exit check and LeakSanitizer reject **579,268 bytes in 98 allocations**.

The initial negative-check helper expected a later leak diagnostic in the IME
case; the actual earlier native failure remains recorded. The keyboard helper
then expected symbolized function names in raw sanitizer output; those raw
frames contain offsets. Offline `addr2line` resolves leading view/context
allocation frames, while also emitting DWARF line-table warnings. The final
verification uses actual exits, native assertions, raw leak totals and retained
resolved frames; neither helper failure is reported as a passing sanitizer run.

The default binary rejects connection, loop, loss and replacement requests with
exit2 before native launch. [Source/object/command/native/negative receipt](aetheri-login-replacement-local.json)
and [independent archive readback](aetheri-login-replacement-final.json).

`nexaui-login-replace234.tar.gz` retains 404 regular members, 21,037,217 bytes,
SHA-256 `ec80cac9373e6a6519d7f3c3040f59761f0e8cc135a5744c953896bfdc0135c9`.
It contains frozen final inputs, original license, binaries/objects, full logs,
both pilot preimages, negative variants and verification helpers. Every member
was read back and hashed twice. Borrowed font, OS headers and private tools
are fingerprinted separately.

The unchanged SDK's seven required
[platform/package CI jobs](https://github.com/Lulus792/NexaUI/actions/runs/43760877115)
remain source-bound to `368cd382`. These standalone consumer checks establish
the new fixture behavior; the SDK CI run does not build this consumer.

## Remaining acceptance

This adds native replacement evidence for two queued answer operations in one
ordinary window. Actual daemon pidfd death, real service/configuration/greeter/
effective-policy admission and trusted placement remain external integration
gates. Other replacement phases, IME providers/candidate geometry, languages,
physical/human/performance acceptance and the full 103-ID catalogue remain open.
Aetheri can adopt this publication/lifetime sequence independently with its own
admitted connection and policy. The same-process synthetic peer does not grant
production trust or prove session readiness. No installed API or version changes.
