# Fresh keys after an input-method transition

Checked SDK source `368cd38292498bd538964ba514ff02e0e9924358`, development
version 0.1.33. The key-state change starts at `f99fa1c30686349def4cfb6fdaa8bc23a1580aae`;
the final source also preserves repeat during an active editor's context refresh.
The original Login inputs remain Aetheri `6eda6feb98b37048ca84d73b65e24f919297987c`.

## Observed failure and change

The preceding [password-storage checkpoint](wayland-password-storage-report.md)
passed its memory checks but failed all three native cancellation tasks. On
entry to an IME field, the simple provider consumed the release of the Tab key
that had moved focus. After focus cancelled the preedit, the next physical
Shift+Tab reached the window. The retained held-key state marked it `repeat=1`,
so ordinary focus navigation ignored it. The unchanged independent AT-SPI peer
also fails with the previous installed `9056af8` host. A diagnostic host reads
public frame events and records the stale repeat with focus still on Answer.

The adapter now marks keys held across actual IME activation/deactivation and
pauses their local repeat timer. The transition alone produces no key event.
A later physical press of one marked key first retires its stale held state
with a key-up event, then queues the fresh press. A forwarded release clears
the mark normally. Ordinary held keys and local repeat keep their previous
handling. This uses the existing private key-state bytes and adds no allocation,
public field or exported symbol. [Input contract](../concepts/input.md)

This is bounded recovery for the observed missing release. The
[Wayland key contract](https://wayland.freedesktop.org/docs/html/apa.html#protocol-spec-wl_keyboard)
requires a matching logical key-state stream and separately specifies repeat.
The fix does not reinterpret a text commit containing Tab as navigation. Full
Tab/Enter forwarding while this provider owns composition remains a separate
acceptance gate.

## Executed checks

The final wire tests send actual keymap, focus and key events over an owned
Wayland connection. Activation/deactivation cases reproduce the missing release
and require a stale release followed by a non-repeat press. Forwarded-release
and unchanged ordinary-held controls retain their prior streams. A fifth case
updates an active editor's empty surrounding-text snapshot to a nonempty one
while a key is held and requires its existing native repeat timer to continue.

The same final tests fail at both handoff cases against installed `f4590ef`, in
Release and ASan. Their forwarded, ordinary-held and refresh controls pass.
The extra refresh test also rejects the first `f99fa1c` implementation: its v3
reenable unconditionally paused repeat during a same-editor context refresh.
The final source marks keys only when that editor was not already active.
The actual failed review run and its preimage remain retained.

Alpine-Dev/Clang22.1.3 passes **78 SDK CTests**, **three installed Login CTests**
and **nine native Login tasks** across static Release, shared Release and
Debug `-O1` ASan/UBSan/LSan. Positive sanitized tasks retain leak detection and
halt-on-error without suppressions. The original native peer, Compose keys,
Shift+Tab return, exact response checks and 35/60-second and 100-frame limits
are unchanged. Six tasks use real simple-IME composition; each native task
closes after exactly two synthetic original-model responses. The shared package
resolves its selected SDK and retains the same 546 exports and public headers.
[Exact source, commands, compiled objects and raw archive](wayland-ime-key-handoff-local.json)

The password checkpoint retains its three failed native tasks. This checkpoint
supplies the later passing tasks; it does not rewrite those historical results.
The nine earlier `f99fa1c` native tasks also pass, but its failed refresh control
prevents treating that intermediate source as the accepted final change.

The archive `nexaui-ime-handoff204.tar.gz` contains 174 verified regular members,
59,559,060 bytes, SHA-256
`2eeb186ca93353325cbaa4535f48f18c6fdccaa0cc5eacc014e60c2fcfa63388`.
The separate password archive and receipt are bound by hash. Borrowed fonts and
external Weston/keymap executables remain outside these source/artifact archives.

All seven required SDK/package CI jobs pass on this exact source in
[main run 43760877115](https://github.com/Lulus792/NexaUI/actions/runs/43760877115),
attempt 1. Windows static/shared execute 149/148 CTests, macOS 125/124,
Linux 282/280, and sanitizers 282 in each driver discovery order. Linux static
also passes the separate presentation-lifecycle repeat. No CTest is skipped;
the optional Windows-ASan and fuzz-campaign jobs are skipped. The standalone
original Login fixture has the separate local evidence above.
[CI receipt and verified raw-log archive](wayland-ime-key-handoff-ci.json)

## Remaining scope

Candidate geometry, broader language/provider workflows, other native-platform
storage and application/protocol/OS copies remain open. The simple provider's
Tab-as-text behavior still requires another provider for full keyboard/IME
acceptance. Trusted placement, real service admission, human assistive use,
physical display and performance acceptance keep their separate host/product
gates. Aetheri's source, pin and application state are unchanged.
