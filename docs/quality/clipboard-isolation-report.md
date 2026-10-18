# Windows clipboard fixture isolation

Source `dc64630b46147a6a880e1f9da4d08cc3f9142b80`, 2026-10-18.
[Local logs, native cases, controls and sources](clipboard-isolation-local.json).

[CI run39101951803](https://github.com/Lulus792/NexaUI/actions/runs/39101951803)
passes all seven required jobs on this exact source: Windows static/shared each
113/113, macOS each94/94, Ubuntu static/shared and sanitizers each108 passed plus
7 Wayland skips. Both Windows jobs execute all three clipboard cases without
skips. Both consumer forms build/link and the installed consumer executes.
Optional Windows-ASan/fuzz jobs skip; the actual local ASan result below is
separate. [Decoded-log hashes and per-case CI receipts](clipboard-isolation-ci.json).

The local transfer test now executes successfully instead of skipping when the
unnamed test window station already exists. The fixture opens that existing
station only after error183, verifies that it is hidden and distinct from the
process's original station, and creates its own desktop. It requires an empty
clipboard there. It never falls back to the interactive clipboard.

The change is limited to the Windows test fixture and CTest registration.
Production clipboard functions, public API, renderers and app sources are unchanged.
The [earlier af5c297 full-suite failure](context-text-density-report.md) retains
its unresolved cause; this isolates the later skip, not that historical failure.

## Evidence and coverage

The native station probe reports an original visible WinSta0 and a distinct,
non-visible candidate. The final fixture logs `reused=1 create_error=183` locally.
Microsoft documents [session-derived unnamed stations, reopening and create-only collisions](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-createwindowstationw).
It also restricts explicitly named stations to administrators; this fixture
requests no named station or new privilege. Station association determines
access to its [clipboard and desktop objects](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setprocesswindowstation).

| Actual local configuration | Transfer | Current-station guard | Busy-clipboard guard |
| --- | --- | --- | --- |
| Static Debug | Passed | Passed | Passed |
| Shared Debug | Passed | Passed | Passed |
| MSVC ASan RelWithDebInfo | Passed | Passed | Passed |

All nine native cases pass, without skips. The ASan target's actual compiler
command contains `/fsanitize=address`; MSVC ASan is not a general leak sanitizer.
These are targeted runs, not three full local suites.

The existing transfer assertions are retained: invalid UTF-8/arguments, the
`Aé😀\nB` sample, native UTF-16 CRLF, query-only UTF-8 length, untouched short
output, complete bytewise roundtrip, text-buffer paste and empty-string transfer.
The current-station guard rejects the actual current object before any clipboard
access. The busy guard writes a known sentinel only into the initially empty,
isolated clipboard, checks that the availability predicate rejects it, and
verifies its complete bytes and native owner remain unchanged.

Two independently compiled safety counterexamples fail at the intended checks:
accepting the current station exits1 in guard mode; treating owned nonempty
contents as available exits1 at the busy-sentinel stage. A separate availability
control disables station reuse and returns77 after error183, reproducing the
isolation skip. That control is not a successful transfer or a product failure.
Manual counterexamples use `/MDd` with the Debug core; an initial mixed-CRT link
warning was corrected before the retained final controls.

## Lifetime and reproduction

A reused hidden station is separate from the user's interactive station, not
exclusive to this process. A busy/unavailable clipboard still skips77 without
reading its contents. CTest serializes the transfer/busy cases with a clipboard
resource lock. These locks coordinate this CTest run, not unrelated processes.
No clipboard contents or station-specific logon names are logged.

After owned clipboard cleanup and window destruction, the fixture restores the
original process station and thread desktop, then closes its owned desktop and
station handles. All six local fixture executions report successful restoration
and both closes. Borrowed original handles are retained. This follows the
[thread-desktop association](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setthreaddesktop)
and [desktop-close](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-closedesktop)
contracts; cleanup failure fails the test.

```powershell
cmake --build build --config Debug --target nexaui_clipboard_tests --parallel 4
ctest --test-dir build -C Debug -R '^nexaui_clipboard' --no-tests=error -V
cmake --build build-shared --config Debug --target nexaui_clipboard_tests --parallel 4
ctest --test-dir build-shared -C Debug -R '^nexaui_clipboard' --no-tests=error -V
```

The local JSON also records the actual ASan command/environment, frozen baseline,
station probe, counterexample sources/executables and hashes. Tests use the
original15-second timeout. Four unused manual objects and one superseded initial
successful log are removed (51,803 bytes); final logs, negative controls,
programs, source snapshots and earlier unresolved evidence remain.

Human copy/paste between applications, physical monitor/IME use, Narrator/
VoiceOver/Orca acceptance and the whole NexaCloud composition remain P2 work.
[R01 startup acceptance](d3d12-flush-timing-report.md) remains open separately.
No NexaCloud sources/pin, driveE:, cache policy or OS/security settings changed.
