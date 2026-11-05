# Aetheri-owned Settings compatibility probe

This isolated C17 test builds the unmodified Aetheri-owned Settings model,
presentation and scene test from an explicitly supplied source snapshot against
an installed NexaUI 0.1.19 candidate. It is a forward-compatibility experiment:
Aetheri's own build deliberately selects 0.1.18 **exactly**, and this probe does
not change that policy, select a development owner or modify Aetheri's checkout.

For the documented test, archive Aetheri commit
`d242c7abf944e08f2693de0e1848f542be107981` with `src/settings` and
`tests/test_settings_view.c`, and extract it to a separate directory. Configure:

```sh
cmake -S tests/aetheri_settings_candidate -B /tmp/settings-candidate \
  -DAETHERI_SOURCE_DIR=/path/to/aetheri-snapshot \
  -DAETHERI_SETTINGS_TEST_FONT=/path/to/font.ttf \
  -DCMAKE_PREFIX_PATH=/path/to/nexaui-sdk-0.1.19
cmake --build /tmp/settings-candidate
ctest --test-dir /tmp/settings-candidate --output-on-failure
```

The original test checks model ownership, semantic controls, keyboard focus,
palette contrast, 100/150/200% model text, several window sizes and raster
densities. It is a scene test: it neither starts an Aetheri compositor nor
proves actual AT-SPI delivery, physical presentation or human acceptance.
Record the exact archive, selected font, SDK artifacts and results when using a
different candidate; a successful build is not approval to change Aetheri's pin.

With the private Weston fixture and `xdotool`, the same unchanged native process
can be exercised using virtual keyboard input:

```sh
xvfb-run -a python3 tests/run_weston_native.py -- \
  python3 tests/aetheri_settings_candidate/native_keyboard.py \
  /tmp/settings-candidate/aetheri_settings_candidate_native /path/to/font.ttf /path/to/xdotool
```

The driver waits for each observed focus change and model result; it opens
Appearance, selects high contrast and 200% text, opens Language, chooses German
and closes with Escape. It uses fixture displays and one ordinary Weston window.
It does not exercise Aetheri's compositor, a physical keyboard or an IME.

When Linux AT-SPI development files are present, the build also provides an
independent `aetheri_settings_candidate_client` and the unchanged Aetheri
`aetheri_settings_candidate_native` process. With the existing private Weston
fixture configured as in `tests/run_weston_native.py`, run:

```sh
NEXAUI_AETHERI_SETTINGS_FONT=/path/to/font.ttf xvfb-run -a \
  python3 tests/run_weston_native.py -- \
  python3 tests/run_atspi_native.py /path/to/at-spi2-registryd \
    /tmp/settings-candidate/aetheri_settings_candidate_client \
    /tmp/settings-candidate/aetheri_settings_candidate_native
```

The client requests contrast, 200% text and German through AT-SPI, checks the
owner's diagnostic model and closes the process. It uses Aetheri's fixture
display records, not device configuration. Set `NEXAUI_SETTINGS_DISABLE_AX=1`
for the negative case: the submitted default view must remain undiscoverable,
then terminate normally. Private Weston/virtual surfaces are not the actual
Aetheri compositor, physical input or a human screenreader review.
