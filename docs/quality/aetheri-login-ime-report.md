# Native login composition and focus cancellation

Consumer `208d453f722baa4c114563cc854eb8b6e1f548dd` uses installed NexaUI
0.1.33 from `9056af80fb2ccf2be0e2839e9cef76ba46cc8660` and the original Aetheri
conversation from `6eda6feb98b37048ca84d73b65e24f919297987c`.

The example's runner starts an owned Xvfb seat, private Weston and AT-SPI bus.
Its Compose mode selects `weston-simple-im` and configures US plus right-Alt
Compose. This environment uses the existing `xkbcomp` to install the private
keymap. An available `setxkbmap` can supply the same setup. The host observes
native preedit length after building the view and publishes state markers with
the original model still waiting for an answer. A separate libatspi peer reads
the visible field and checks the password role and suppressed interfaces.

The commit task observes secret preedit, types the final Compose key and then
submits the exact response `á` through Answer. The visible prompt receives the
same Compose sequence. Its committed Text interface stays empty during preedit
and its observed value becomes `á` after the commit. The cancellation task moves
focus from the secret field through the native accessibility interface, observes
preedit removal, returns to the field and types `a`. The original conversation
must receive exactly `a`. It then executes the visible Compose sequence. Both
tasks require retired field references and a clean window close after two exact
synthetic requests.

Three headless CTests and nine native tasks pass across static Release, shared
Release and Debug `-O1` ASan/UBSan/LSan on Alpine-Dev/Clang 22.1.3. Each profile
runs the original keyboard workflow, the Compose task and the cancellation task.
Native runs retain the 35-second client, 60-second Weston and 100-frame limits.
The peer bounds its captured host transcript at 128 KiB. A separate run with the
ordinary Weston keyboard provider and the same binaries exits 1 at the required
preedit marker. [Source, artifact and command receipt](aetheri-login-ime-local.json)

The first Compose attempt failed at the original Tab traversal. Its diagnostic
wire trace records `simple-im` sending Tab as a committed control character.
The IME tasks therefore use standard AT-SPI focus for the first field exit and
focus cancellation, and Action for submitting an answer. Their remaining button
traversal, Compose keys and Escape use the private keyboard. The keyboard task
continues to use Tab, Shift+Tab and Return throughout. The failed binaries,
source preimage and protocol trace are retained. Acceptance of full Tab/Enter
forwarding through another installed input-method provider remains open.

The archive `nexaui-login-ime180.tar.gz` has 9,501,319 bytes and SHA-256
`9993fa5dc3104a62a83cc4c58465da088fb077425003de948134aee9ea64883c`.
All 121 members were read back and verified. The font, Weston helper and keymap
compiler are recorded by hash and supplied separately. The fixture builds
against the same three installed SDK libraries as the prior input-storage
checkpoint. [Reproduction instructions](../../examples/37_aetheri_login/README.md)

All seven required SDK/package CI jobs pass on this exact source in
[run 43756393470](https://github.com/Lulus792/NexaUI/actions/runs/43756393470),
attempt 1. Windows static/shared execute 149/148 CTests, macOS 125/124,
Linux 273/271, and sanitizers 273 in each driver discovery order. Linux static
also passes the separate presentation-lifecycle repeat. No CTest is skipped;
the optional Windows-ASan and fuzz-campaign jobs are skipped. These counts cover
the SDK and package consumers; the standalone original Login fixture has the
separate local evidence above. [CI receipt and verified raw-log archive](aetheri-login-ime-ci.json)

This advances packet 6 for T04/T06, I01, A01/A02/A06, P05 and Q02/Q08. IME
candidate geometry, native IME storage lifetime, broader language/provider tasks,
human assistive use, Aetheri trusted placement and actual service admission
remain open.
