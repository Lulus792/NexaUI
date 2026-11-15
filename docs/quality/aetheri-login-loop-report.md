# Original connection readiness and native loss

Consumer `2615f6f72d8b96615397d92ac61bc9f9601e99ce` extends example37 with
optional Linux reference event-loop glue. It uses installed NexaUI 0.1.33 from
`368cd38292498bd538964ba514ff02e0e9924358` and the unchanged eight original
Aetheri inputs from `7d5fa4723c13460e9d61dc7db33b6a521409053f`.
Those selected inputs also remain unchanged at independently advancing Aetheri
`a902bd900d45cb9d6eeec95289f98181ad3766be`. No Aetheri checkout or SDK pin changes.

## Host contract

The optional [connection poller](../../examples/37_aetheri_login/connection_poll.h)
borrows an independently admitted original connection and an optional readable
application wake fd. It owns only its epoll descriptor. Original socket and
peer-pidfd watches feed existing public `nx_window_poll_with_fd_events`; writable
interest exists only while the original connection has output. One step makes
at most one original pump call. There is no second conversation model, service
connection or admission authority. Use one active poller per connection, on its
owner thread, and drive transport through it while active.

The host sets an absolute monotonic deadline when a request is queued and keeps
it through partial progress, native input and assistive traffic. Expiry calls
the original interrupt. Terminal original errors retire the service watches;
window, assistive and application-wake handling continue. Rebuild and publish
from original status when `changed` is set. The host drains its own wake fd.
Close the poller before its borrowed resources. Closing it neither cancels nor
closes the connection. [Build and adoption instructions](../../examples/37_aetheri_login/README.md)

## Executed checks

Alpine-Dev, Clang22.1.3, Linux6.18.33.2 WSL2 and uid65534 pass **16 consumer
CTests**: five each in static Release, shared Release and Debug `-O1`
ASan/UBSan/LSan, plus one default minimal-input build. Own C17 sources retain
extensions OFF and strict warnings including conversion/shadow/Werror. Only
the unchanged imported connection source has the previously documented musl
mixed-sign warning exclusion. Sanitized runs retain leak detection and no
suppressions. The installed SDK libraries match their prior verified hashes.

The extended lifetime case destroys/recreates the view before sending a queued
create-session request, then before sending a queued UTF-8 answer. The original
connection and pending bytes survive. Each request reaches the peer exactly
once; opening/closing the poller adds no cancellation or replay. This is a
rendered consumer case, not native attached-view replacement evidence.

**36 private native tasks** pass across the same three profiles:

| Path | Tasks | Observed behavior |
| --- | ---: | --- |
| Existing free conversation | 9 | Keyboard, Compose and composition cancellation |
| Inline original connection | 9 | The same workflows through original transport |
| Original readiness loop | 9 | The same workflows through socket/pidfd aggregation |
| Loop EOF, declined admission and deadline | 9 | Protected composition retires; unknown state remains closable |

The separate libatspi client links neither the SDK nor original model. Existing
ordinary assertions remain; new loss branches wait without further typing for
the published unknown state, check Defunct secret objects and absent Answer,
then close with Escape. Actual original checks, AF_UNIX streams, self-process
peer pidfds and `PIDFD_GET_INFO` run in the synthetic fixture. The private
parent/child control pipe acts only on that synthetic peer/callback. It does
not grant production admission. EOF and declined admission submit zero answers;
the timeout case submits one exact answer and withholds its reply. Logs record
original errors -104/-13/-125 and zero live preedit after retirement. Ordinary
workflows submit two exact responses. The 35-second client, 60-second Weston
and 100-frame bounds remain unchanged. The fixture's 150ms request deadline is
a behavior check, not a performance guarantee.

Two private compiled counterexamples configure/build successfully and fail the
same native client's unknown-state assertion with exit1: removing the original
pump hides EOF; removing the request deadline leaves timeout waiting. Neither
uses an outer timeout as its expected failure. The default binary separately
rejects each requested connection/loop/loss feature with exit2 before launch.
[Exact inputs, compiled objects, commands and logs](aetheri-login-loop-local.json)
and [archive readback, environment and default guards](aetheri-login-loop-final.json).

The raw archive `nexaui-login-loop222.tar.gz` contains 329 regular members,
16,269,339 bytes, SHA-256
`b6bd97087d30bf303251529b8fcf42c4cd5bcc21181feb6f1a0670b385e4c365`.
It retains the frozen final inputs, pilot preimage, original license, objects,
binaries, build/test/native logs and both negative variants. Every member was
read back and hashed, then independently checked again before publication.
Borrowed font, OS headers and private tools are fingerprinted separately.

The SDK implementation is unchanged from `368cd382`; its seven required
[platform/package CI jobs](https://github.com/Lulus792/NexaUI/actions/runs/43760877115)
pass with the [source-bound SDK receipt](wayland-ime-key-handoff-ci.json).
The standalone consumer is covered by the checks above, not that SDK CI run.

## Remaining acceptance

Production service/loaded-configuration/greeter/effective-policy admission and
trusted compositor placement remain host gates. Actual daemon pidfd exit and
native attached-view replacement require separate integration evidence. Other
IME providers, candidate geometry, languages and physical/human/performance
acceptance remain open, together with the full 103-ID catalogue. Aetheri can
reuse this opt-in host glue with its own admission and lifetime policy; these
synthetic tasks do not establish production readiness or session readiness.
No new installed API, dependency, SDK version or 1.0 release is introduced.
