# Original admitted-connection presentation

Consumer `fbd5a00b934e0a43a12ced8ccafe627170077d4b` adds an optional borrowed
connection to example37. It uses installed NexaUI 0.1.33 from
`368cd38292498bd538964ba514ff02e0e9924358` and unchanged original Aetheri inputs
from `7d5fa4723c13460e9d61dc7db33b6a521409053f`. The selected eight Aetheri files
also remain unchanged at independently advancing `2ce5c7fae7ac5db5bca2162bbfbc160bd76244a9`.

## Ownership and behavior

`NEXAUI_LOGIN_WITH_CONNECTION_MODEL=ON` compiles the original `connection.c`
and enables `aetheri_login_view_create_connection`. The host supplies a connection
after its independent admission. The view reads its local status and routes
explicit begin/answer/cancel/start requests through the original connection
methods and their mandatory checks. It creates no parallel conversation model.
The host retains watch/pump, deadlines, interruption and connection close.
Destroy the view before the connection. Replacing the view leaves the original
conversation alive; a null connection displays unavailable.
[Build and host instructions](../../examples/37_aetheri_login/README.md)

Status is a local observation. Rendering does not perform a fresh admission
check. The host must drive actual observations and transport loss into the
original connection. A failed command check permanently fences that connection;
the view retires its protected field and shows unknown state. Restoring the
fixture's callback result cannot silently rearm the old conversation. A session
acknowledgement remains historical after EOF and is shown as acceptance of a
request. This UI does not establish session readiness or trusted placement.

## Executed evidence

Alpine-Dev/Clang22.1.3, C17 with extensions disabled, passes **16 installed
consumer CTests**: five in each static Release, shared Release and Debug `-O1`
ASan/UBSan/LSan build, plus the default minimal-input build. The new cases use
actual AF_UNIX streams, native peer pidfds and `PIDFD_GET_INFO` with a
same-process synthetic peer. They cover fragmented replies, exact Unicode,
empty response versus null acknowledgement, explicit session request, historical
acknowledgement after EOF, queued UI action after loss, failed command admission,
protected semantics and view replacement while the model awaits a response.

**18 private native tasks** pass: nine through the original free model and nine
through the original connection, across the same three profiles. Each runs the
unchanged independent libatspi peer with ordinary keyboard, real simple-IME
Compose and focus cancellation/Shift+Tab return. Each closes after two exact
synthetic responses. The existing 35-second client, 60-second Weston and
100-frame bounds remain. The peer still checks protected interfaces, committed
visible text and retired objects. Connection tasks record more than ten actual
original check calls; their callback admits the synthetic setup only.

A separate copy that optimistically accepts an answer without invoking the
original method fails the admission case. The default native build rejects an
explicit connection-mode request with exit2. Positive sanitized tasks retain
leak detection without suppressions. The default build uses the prior minimal
six-file export, with the connection feature off. Its library has no connection
constructor. [Exact sources, commands, compiled objects and artifact receipt](aetheri-login-connection-local.json)

The initial Alpine build lacks `sys/pidfd.h`, new socket constants and a
compatible combined libc/kernel fcntl include. The retained failed build leads
to the private header adapter now used for that profile: installed pidfd UAPI
structures, preserved ioctl request bits and Linux socket ABI constants. Their
definitions follow [Linux v6.18 socket constants](https://github.com/torvalds/linux/blob/v6.18/include/uapi/asm-generic/socket.h)
and [SCM_PIDFD](https://github.com/torvalds/linux/blob/v6.18/include/linux/socket.h).
No pin/check emulation is supplied. Only the imported connection source's musl
`CMSG_NXTHDR` mixed-sign comparison warning is excluded; the view, tests and
native host keep their strict warnings. All original source bytes are retained.

The verified archive `nexaui-login-connection215.tar.gz` contains 251 regular
members, 15,342,504 bytes, SHA-256
`c76a21b73ff5f919e53e7c550bd6b0665d7a973e6c57a67f40598901f36ea51b`.
It retains the initial failed profile, corrected pilots, negative source and
binaries, actual objects, native logs and original license. Borrowed fonts,
external tools and installed OS headers are fingerprinted separately.

The SDK implementation is unchanged from `368cd382`. Its seven required
[platform/package CI jobs](https://github.com/Lulus792/NexaUI/actions/runs/43760877115)
pass with the [source-bound CI receipt](wayland-ime-key-handoff-ci.json).
The optional consumer has the standalone checks above.

## Remaining acceptance

Actual service/loaded-configuration/greeter/effective-policy admission, trusted
compositor placement and real-service watch/pump/deadline behavior remain host
integration gates. Queued-send view replacement and native loss presentation
need further targeted consumer evidence. The synthetic callback and ordinary
Wayland window do not prove those gates. Other IME providers, candidate geometry,
physical/human/performance acceptance and the remaining catalogue stay open.
No Aetheri implementation, active SDK pin or 1.0 release approval changes here.
