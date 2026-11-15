# Original Aetheri login conversation view

Consumer `9aa87967364e4e475bcd64154891b57779f645f4` compiles the original
Aetheri conversation from `6eda6feb98b37048ca84d73b65e24f919297987c` against
installed NexaUI 0.1.33 from `529d5a9d102d975f4fa0664df4936d343bc63b58`.
The standalone C17 target connects original prompt and request states to
NexaUI's existing text, focus and semantics APIs. [Consumer and run instructions](../../examples/37_aetheri_login/README.md)

The headless workflow checks unavailable service, empty account rejection,
keyboard submission, visible answers, protected responses, explicit information
acknowledgement, authentication denial, cancellation and uncertain transport
loss. It checks the exact synthetic requests, including the difference between
an empty secret answer and an information acknowledgement. It injects a queued
semantic action before a transport interruption and verifies that the next
frame removes the credential controls without producing a request. Repeated
secret prompts use fresh IDs and a fresh draft. The session request stays
separate from authentication and its acknowledgement is rendered as acceptance
of that request. The tests inspect NexaUI's semantic state directly and execute
the software renderer for every checked frame.

Clang 22.1.3 on Alpine-Dev ran one consumer CTest per installed profile, for
**three passing runs** in static Release, shared Release and Debug `-O1`
ASan/UBSan/LSan. The initial adapter had a signedness warning and the first
runtime setup passed zero as a button label layout width. The final code uses
a boolean conversion and the available button width. Both failure logs are
retained. The receipt records source, library, compiled object and executable
hashes and the shared executable's library search path. [Local receipt](aetheri-login-view-local.json)

The repository's platform CI is tracked separately. The installed login
consumer tests are the local runs cited above. [Main CI run](https://github.com/Lulus792/NexaUI/actions/runs/43747562669)

The local archive `nexaui-login-view159.tar.gz` contains 4,272,147 bytes.
SHA-256 is `d8699d8d8e581cc48037e017c35467288a564a29ddc351e95c26d8dce135dfd8`.
All 45 members were read back and verified.

This is a packet 6 checkpoint for D06/T06/A01/A06/P05/P08/Q08. The sample's
fixed message region clips long prompts. Native compositor and IME integration,
independent AT-SPI validation, credential lifetime inside native input queues,
trusted placement and login service admission still require evidence. The
Aetheri owner remains responsible for authentication, transport and eventual
session startup.
