# Native peer-process exit without socket EOF

Consumer `eddad298db1058e9a53b39944e892889537a5d66` extends example37 with a
separate owned credential-peer process. Installed NexaUI remains 0.1.33/source
`368cd38292498bd538964ba514ff02e0e9924358`. All eight original Aetheri inputs
remain byte-identical to `7d5fa4723c13460e9d61dc7db33b6a521409053f`, including
the connection's actual kernel pin checks. The same selected files remain
unchanged at independently advancing Aetheri `a902bd900d45cb9d6eeec95289f98181ad3766be`.

## Process and stream lifetime

`--ime --connection-loop --connection-loss peer-exit` forks a private child
before creating the native window/assistive bridge. That child creates both
AF_UNIX stream endpoints, transfers the client endpoint and a remote keeper
through an owned descriptor-transfer channel, closes its client alias and
confirms completion. The parent checks the stream's kernel credential PID
against its own fork result and obtains the independent pidfd from that known
child. The unchanged original constructor additionally verifies its actual
`SO_PEERPIDFD`, pidfs identity and `PIDFD_GET_INFO` credentials.

The synthetic parent harness alone drives the existing exact protocol bytes.
The child is the real kernel-credential peer, not an Aetheri daemon
implementation. Its parent-owned remote keeper intentionally prevents EOF when
the child exits. This arrangement distinguishes process death from the earlier
self-process socket-close fixture; it does not supply production admission.

After observing protected secret preedit, the independent libatspi client asks
the owned child to exit. The original event loop detects process loss and fences
with `-ESRCH`. The host verifies readable pidfd, no readable/hangup/error
condition on the polled original client stream, an empty remote keeper and
child exit0. It reaps the child, retires the composition, publishes unknown state and
keeps Escape close usable. The original connection owns its client stream;
fixture channels, remote keeper and child pin retain separate explicit owners.
Failure cleanup closes its channels and reaps only its own child. A child
parent-death signal also covers probe-driven host termination.
[Build/run instructions](../../examples/37_aetheri_login/README.md)

## Executed checks

Clang22.1.3/Alpine-Dev on Linux6.18.33.2 WSL2, uid65534, passes **16 consumer
CTests**: five each in static Release, shared Release and Debug `-O1`
ASan/UBSan/LSan, plus one default minimal-input build. Own C17 sources retain
extensions OFF, conversion/shadow/Werror and the prior strict flags. Only the
imported original musl mixed-sign warning has its existing scoped exclusion.
Sanitized positives keep leak detection without suppressions; installed SDK
library hashes remain unchanged.

**48 native tasks** pass: all preceding 45 ordinary/IME/cancellation/queued-view-
replacement/EOF/admission/deadline workflows plus three credential-peer process
exit cases, one per profile. The new cases observe secret preedit before loss,
zero submitted answers, unknown state, retired protected objects, no Answer and
clean Escape exit. Each records `pidfd_readable=1 socket_eof=0 child_exit=0`,
original error -3 and zero live preedit. The independent client links neither
SDK nor original model. The 35-second client, 60-second Weston and 100-frame
bounds are unchanged. This is a native virtual-seat/process-lifetime check,
not physical display, latency or production trust evidence.

A private copy of original `connection.c` deliberately omits the current peer
verification on operation entry, retaining constructor and receive checks. It
configures/builds successfully but fails the same native client's unknown-state
assertion with exit1: the remote keeper prevents EOF, and the view stays in its
prompt despite child death. All positive original bytes and the actual Aetheri
checkout are unchanged. This diagnostic bypass is retained only as a negative
artifact. Initial negative-helper preflights rejected an ambiguous two-match
mutation needle and a generated quoting error before execution; the corrected
generator, failed preflight files and actual native failure remain retained.

A pre-publication contract review replaced the client-stream peeking diagnostic
with poll-only observation, as required for the borrowed descriptor. The final
source was rebuilt and the complete positive/negative native matrix repeated.
The earlier `26e47e4` receipt, readback and fixture preimage remain retained;
its archive fingerprint is verified before creating the final evidence archive.

The default binary rejects all four optional feature flags and separately
rejects `peer-exit` loss, each with exit2 before native launch.
[Source/object/command/native/negative receipt](aetheri-login-process-local.json)
and [independent archive readback and executable scan](aetheri-login-process-final.json).

`nexaui-login-process251.tar.gz` contains 411 regular members, 16,026,749 bytes,
SHA-256 `f5880b0336e83063f5508eea209e35edda9f9c2a4f8191fdb5c8b184247aa721`.
It retains the final frozen inputs, original license, compiled objects/binaries,
pilot preimage, complete build/test/native logs, private negative source and
helpers. Every member was read back and hashed twice. Borrowed font, OS headers
and private tools are fingerprinted separately. No selected fixture executable
is observed live in the final scan; ordinary positives also require child reap.

The unchanged SDK's seven required
[platform/package CI jobs](https://github.com/Lulus792/NexaUI/actions/runs/43760877115)
remain bound to `368cd382`. The standalone consumer has the checks above; that
SDK run does not establish this new fixture behavior.

## Remaining acceptance

This closes the missing scoped evidence for a separate credential-peer process
dying while its remote stream remains open. Actual Aetheri daemon/service,
loaded configuration, greeter/effective-policy admission and trusted placement
remain host integration gates. Other native replacement phases, IME providers,
candidate geometry, physical/human/performance acceptance and the full 103-ID
catalogue remain open. Aetheri can independently reuse the existing host loop
with its own admitted connection and lifecycle policy. No installed API, SDK
version, active pin or 1.0 release changes.
