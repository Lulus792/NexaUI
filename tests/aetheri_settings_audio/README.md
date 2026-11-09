# Original Settings audio-entry composition fixture

This standalone C17 test consumer uses an installed NexaUI package and an
explicit immutable Aetheri source export. The original `settings/audio.c`,
`audio_host.c`, `nexaui_main.c`, view and model are compiled unchanged. The test
executable supplies only controlled original session-control observations and
effects. It performs no service/device operation or production admission.

Enable the [original audio entry](../../examples/21_aetheri_model/README.md) through
an existing caller-defined target. Here that target links only GLib and the
original model; it does not link the real audio-session backend. Systemd headers
are required by the original application's `audio/session_control.h`. Their
explicit SDK path is a test-consumer input, not an installed NexaUI dependency.

Supply the complete original-view/model export described by Example21, plus
`src/settings/audio.c`, `audio.h`, `audio_host.c`, `audio_host.h`,
`src/audio/control.h`, `session_control.h`, and the unchanged
`tests/test_settings_audio.c`, `test_settings_audio_host.c`. Keep the original
license notices with the export. The caller-defined host and model must come
from the same original source generation.

```sh
cmake -S tests/aetheri_settings_audio -B /tmp/settings-audio-consumer \
  -DCMAKE_PREFIX_PATH=/path/to/nexaui-0.1.32-install \
  -DAETHERI_SOURCE_DIR=/path/to/original-aetheri-export \
  -DAETHERI_SYSTEMD_INCLUDE_DIR=/path/to/header-sdk/usr/include
cmake --build /tmp/settings-audio-consumer
ctest --test-dir /tmp/settings-audio-consumer --output-on-failure
```

The unchanged original binding test verifies exact revisions/targets, correlated
results, no replay and loss fencing. The unchanged events test uses actual GLib
I/O, timers, threads, epoll/poll and descriptor retirement, retaining its original
15-second CTest deadline. Neither creates a real audio service.

`controlled_audio_settings` runs only under the existing private Weston fixture
with an inherited `SOCK_SEQPACKET` test channel. Its original window loop owns
the model and routes actual native input/actions; controlled observations arrive
as owner-context GLib I/O. The independent `controlled_audio_atspi` peer links
system libatspi/GLib, with no NexaUI or Aetheri model linkage. It requests a volume
increment, checks one pending submission and unchanged observation, acknowledges
explicitly, then exercises mute/default-output requests, owner loss and no replay.
`NEXAUI_AUDIO_CLOSE_PENDING=1` selects close while an operation remains pending.
The controlled owner must still report pending/unknown when the original window
returns, before this test drops its own source/context.
The source contains no reconnect or real backend factory.

Run it on the existing private Weston/Xvfb seat with a private AT-SPI registry:

```sh
NEXAUI_PLATFORM=wayland NEXAUI_AETHERI_SETTINGS_FONT=/path/to/font.ttf \
xvfb-run -a python3 tests/run_weston_native.py -- \
  python3 tests/run_atspi_native.py /path/to/at-spi2-registryd \
    /tmp/settings-audio-consumer/controlled_audio_atspi \
    /tmp/settings-audio-consumer/controlled_audio_settings
```

With installed0.1.33, add `NEXAUI_AUDIO_NUMERIC_VALUE=1` to exercise the standard
AT-SPI Value interface as well: real range/increment/current/Text reads, the
unmodified libatspi absolute setter, exact raw invalid/disabled/Defunct errors,
and independent double-valued change signals. Both full observed/loss and
close-pending variants keep the existing deadlines/frame/transcript guards.
libatspi2.60.5 aborts on an error reply and does not decode double event payloads;
the [transport notes](../../docs/concepts/atspi.md#numeric-value-controls) explain
the retained actual failure and independent wire checks. No client library or
SDK error contract is weakened to make a refusal look successful.

Keep sanitizer instrumentation and leak detection enabled for matching SDK and
consumer builds. Record source/font/library hashes, build flags, actual native
outcomes and failure logs. These tasks do not prove actual Aetheri compositor,
admitted session/backend integration, audibility/persistence, broader numeric
control kinds/UIA/AX, canonical fonts, physical input or human screenreader
acceptance. The original process owner retains deferred session retirement after
the window returns; this controlled fixture has no asynchronous real backend.
