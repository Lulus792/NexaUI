# Native Wayland login conversation

Consumer `e8abe04b4b43d7a23f95d8a33e5cf59b2e52aa4c` compiles original Aetheri
`6eda6feb98b37048ca84d73b65e24f919297987c` against installed NexaUI 0.1.33
from `529d5a9d102d975f4fa0664df4936d343bc63b58`.

The optional fixture opens a Wayland window with the public software presenter
and publishes the existing login view. A separate libatspi process discovers
that window by child PID on a private registry. The original model processes
fixed synthetic prompt replies and consumes the actual queued responses. The
host logs only the result, number of requests and number of frames.

On the private Weston seat, Tab and Shift+Tab traverse the password, Answer
and Cancel. Before typing and again after focus traverses away and back, the
client sees the password role and absence of the value attribute, Text and
EditableText. Return sends the exact synthetic response. The old password is
Defunct while the new entry is focused. Native typing changes the visible
entry's observed value, Return sends a second exact response and retires the
entry, and Escape exits after semantic detachment and renderer teardown.

On Alpine-Dev under Clang 22.1.3 with Weston 14.0.2 and libatspi 2.60.5, static
Release, shared Release and Debug `-O1` ASan/UBSan/LSan each pass one headless
CTest and one private native workflow. Each native run exits with exactly two
requests and 53 frames. The unchanged release peer rejects a private build
whose focused field uses ordinary text semantics, before it types the answer.
The native executables each return 2 without the private fixture environment.
The default build compiles neither native executable and passes its headless
CTest. [Build and native receipt](aetheri-login-native-local.json) · [Default and guard checks](aetheri-login-native-guards.json)

`nexaui-login-native169.tar.gz` contains 9,434,950 bytes. SHA-256 is
`d55622ff1c61e890f2d6acb6767704b8dce3af4ad57987b337fcd0751cd9bbbf`.
All 81 members were read back and verified. The borrowed font is recorded by
hash and is supplied separately. Use the example README to reproduce the
fixture. [Run instructions](../../examples/37_aetheri_login/README.md)

The repository's platform CI is tracked separately from the native fixture.
[Main CI run](https://github.com/Lulus792/NexaUI/actions/runs/43748903983)

This advances packet 6 for D06, T06, I01, A01, A02, A06, P05 and Q08. Native
IME, protected event storage, human assistive use, Aetheri's trusted compositor
placement and a real admitted login service remain separate acceptance gates.
