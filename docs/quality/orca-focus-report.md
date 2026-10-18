# Orca focus on newly published controls

Source: `10cd3fb2de28674af7ef493cd0c1f249994efa12`, 2026-10-18.
The [local receipt](orca-focus-local.json) contains commands, hashes, nine actual
Orca checkpoints and the negative cases. This is a bounded NexaUI framework and
native-example check; the separate NexaCloud application/pin was not changed.

## Defect and correction

Opening **Systeminformationen** in the rendered NexaCloud example created a
focused **Schließen** button. Its native AT-SPI state already contained FOCUSED,
but Orca did not announce it within the unchanged 40-second task deadline.
Seven preceding navigation/button checkpoints had actual announcements.

`nx_ax_publish_delta` added new objects to Cache and emitted ChildrenChanged, then
skipped their state deltas. Orca did not treat that cache insertion as a keyboard
focus transition. The adapter now emits `Object.StateChanged("focused", 1)` after
the insertion when the new incarnation is actually focused. Existing unchanged
objects retain their delta path. Native visibility/host-focus requirements still
apply; no text/value interface or speculative application action was added.

The independent libatspi client now requires the focused event and its exact
source path when a control first appears already focused and when its role
changes to a new incarnation. Repeated unchanged frames must add no focus event;
the retired path must return Defunct. The new regression fails against the
previous provider at the first event assertion and passes with the correction.
Protocol reference: GNOME's [native event declarations](https://github.com/GNOME/at-spi2-core/blob/main/xml/Event.xml).

## Executed local checks

| Configuration | Actual result |
| --- | --- |
| Alpine/GCC Debug static | All 5 AT-SPI tests passed |
| Alpine/GCC Debug shared | All 5 AT-SPI tests passed |
| Alpine/Clang Debug ASan + UBSan, leak checks enabled | All 5 AT-SPI tests passed |
| Previous provider + new native regression | Failed at the missing new-control focused event |
| Previous Debug example + actual Orca modal task | 7 checkpoints passed; modal close announcement timed out |
| Corrected Release example + actual Orca task | All 9 native focus/announcement checkpoints passed |
| Local Windows clipboard diagnostic rerun | Skipped with test exit 77; CTest itself returned 0 |

The five native tests cover X11/private registry, session-bus discovery, disabled
bridge, missing runtime ABI and missing bus. The new focus assertions execute in
the two native registry cases. The fixture is renderer-independent; the Orca
task below separately runs the normal font-backed Vulkan example.

## Real Orca task and limits

Environment: Alpine-Dev/WSL 2, Alpine 3.24.2, Orca 50.2, AT-SPI2 2.60.5,
private Xvfb 1600×1200×24, Openbox, private session/accessibility buses and private
settings/runtime directories. PulseAudio has exactly one private null sink.
The independent speech preflight completed BEGIN/END, Orca connected to the real
Speech Dispatcher backend, and its retained audio log has no detected audio
initialization failures. These are not per-control synthesis-completion or human
listening results.

The script sends real X11 keys to the normal rendered example. For every
checkpoint it waits for an active native window, exactly one matching focused
control and the actual matching Orca `SPEECH OUTPUT` entry before continuing:

1. Tab through Übersicht and Dateien; Return opens the real sample file page,
   confirmed by the newly published Bergsee.jpg item.
2. Tab through Übertragungen, Favoriten, Papierkorb, Einstellungen and
   Systeminformationen, requiring each name to be announced.
3. Return opens the dialog and announces its focused Schließen button.
4. Escape restores focus to Systeminformationen and announces it again.

All nine checkpoints passed. Their observed end-to-end times were **5.815–29.902
seconds**. The private software-rendering session also produced registry warnings
about an unresponsive Orca process. The cause of these delays is not isolated;
Release optimization alone did not establish fluent operation. This result
closes the missing-event defect, **not** responsiveness or whole-product
accessibility acceptance. Narrator, VoiceOver, full text/edit/range/table/collection
contracts, physical devices/monitor migration, contrast/reduced-effects tasks,
human use and other Linux desktops remain open. P2 and the broader goal remain
in progress.

Run on Linux with the dependencies listed in the script:

```sh
cmake -S . -B out/orca-release -DCMAKE_BUILD_TYPE=Release \
  -DNEXAUI_BUILD_VULKAN=ON -DNEXAUI_BUILD_TESTS=OFF -DNEXAUI_BUILD_EXAMPLES=ON
cmake --build out/orca-release --target nexaui_nexacloud_native -j4
python3 tests/check_nexacloud_orca.py \
  --app out/orca-release/examples/nexacloud/nexaui_nexacloud_native \
  --font /path/to/your/font.ttf --work out/orca-new-run
```

`--work` must be new so previous evidence is retained. This optional task is not
a default CTest dependency and does not install software or redistribute fonts.
Final native window lookup uses an ASCII prefix, requires one result on the
private display, then checks the complete UTF-8 title; the AT-SPI bus owner must
match the launched process. Exploratory lookup/RPC failures are excluded in the
local receipt. Only this task's child services are terminated. Private runtime
files are removed; logs and hashes remain.

## Clipboard diagnostic boundary

`tests/test_clipboard.c` now reports the environment, failing stage and last
Win32 error without logging clipboard contents. Its assertions and skip policy
are unchanged. Here the unnamed private station returned error 183 and the
interactive clipboard was in use, so the test skipped. Microsoft documents the
session-derived unnamed station and `CWF_CREATE_ONLY` collision behavior in
[CreateWindowStationW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-createwindowstationw).
This explains this run's isolation failure, not the earlier local full-suite
failure. A temporary named-station probe returned Access Denied and was reverted;
no new privilege requirement was introduced. The prior local clipboard failure
remains unresolved; CI and local results must be reported separately.

## CI and handoff

[Run 39089162407](https://github.com/Lulus792/NexaUI/actions/runs/39089162407)
passes all seven required jobs on this exact source. The
[CI receipt](orca-focus-ci.json) records decoded log hashes, actual test lines
and package steps: Windows static/shared each108/108, macOS each93/93, Ubuntu
static/shared and sanitizer each107 passed plus7 Wayland skips (114 registered).
Both native registry cases pass in all three Linux jobs. Both Windows clipboard
tests pass; these do not close the separate earlier local failure. Optional
Windows-ASan and fuzz jobs were skipped. No CI task here runs Orca; native
protocol regression and the local real-Orca task are distinct evidence.

Both consumer forms build/link in the core jobs; the installed consumer also
executes. Add-subdirectory consumer execution is not claimed.

For application adoption, `10cd3fb` includes the earlier `749c7fa` HiDPI/Metal
fixes. The application owner reviews its dependency pin separately. Cached
paint/text ownership and Design-A limits remain as documented in the
[consolidated handoff](design-a-acceptance.md) and
[context text-density report](context-text-density-report.md).
