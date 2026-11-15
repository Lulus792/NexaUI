# Retired Wayland password composition storage

Implementation `65fc3584e84daf8309b806101c4807eb1eada8ad`, checked source
`f4590ef0402b0ca431509600d5f4f05f6aa9b837`, development SDK 0.1.33.

A v3 `preedit_string` replaces the pending reply before `done`. This path
previously freed the old password copy without overwriting it, including when
the replacement was empty or invalid. The other pending-commit, current-preedit
and cancellation helpers already overwrote their password copies. The fix uses
the same volatile overwrite for the full retiring allocation at replacement.
It preserves the current composition until its documented retirement.

The wire regression first locates the actual owned copy by its complete live
bytes through the window allocator. Before freeing that pointer, the allocator
checks that every allocated byte is zero. Cases cover pending replacement,
empty and invalid replacement, promotion at `done`, focus loss, stale `done`,
purpose changes, disable and destruction. Promotion keeps the allocation live;
the lifecycle case observes nine later overwritten allocations.

The final test fails against installed `9056af8` in all three replacement
cases, in both Release and ASan. Its lifecycle control passes there. Static
Release, shared Release and Debug `-O1` ASan/UBSan/LSan on Alpine-Dev/Clang22.1.3
pass 63 SDK CTests and three installed original Login CTests. There are no
public-header changes; the shared library retains the same 546 exported names.
[Commands, source/artifact hashes and archive manifest](wayland-password-storage-local.json)

Six of the nine native Login tasks pass on `f4590ef`: ordinary keyboard and
Compose in each profile. All three cancellation tasks fail at the unchanged
Shift+Tab return to the secret field. The same independent peer fails with the
previous `9056af8` host. Separate protocol traces show the actual physical key
arriving after IME deactivation; a diagnostic host recording public frame events
shows it incorrectly marked `repeat=1`. These failures remain evidence of this
candidate and are not counted as successful native cancellation. A subsequent
key-handoff change addresses that separate state problem.

Preparation retains two failed stricter fixture builds (existing conversion and
unused font-helper warnings) and the first positive run's incorrect test query:
invalid input errors are reported by polling, rather than `next_event`.
The corrected tests use the project's C17 `-Werror` test profile. The library
retains its normal `-Wall -Wextra -Wpedantic` checks. Negative tests that exit at
their deliberately failing assertion disable leak reporting after that abort;
positive sanitized tasks retain leak detection without suppressions.

The verified raw archive `nexaui-ime-storage190.tar.gz` contains 202 regular
members, 58,654,944 bytes, SHA-256
`05dbb4c914b139f7c34b1cf32a69db051602f79838f5424c76eb756f47877106`.
It includes the failing native runs and diagnostic traces, original-model
inputs, executable/object proofs and preparation failures. The borrowed font
and external Weston/keymap tools are supplied separately. This covers owned
Wayland password composition storage; other native platforms and copies in
the application, input method, compositor and protocol transport need their
own audit. Trusted placement and actual service admission remain host work.
