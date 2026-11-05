# Aetheri desktop model: initial dock view

This separate C17 consumer builds against an installed NexaUI SDK and an
explicit Aetheri source snapshot. It borrows the original desktop model and
font bytes; it returns Search, Settings and Terminal intents to the caller.
The view does not start processes, mutate the Aetheri model, create a Wayland
connection or select a privileged role. A pending app request disables its
corresponding button. The host remains responsible for validating and acting
on every intent and observing the real operation outcome. The caller supplies a
nonzero increasing revision, incremented on every model mutation including
remove/re-add with the same ID. Events queued for an older revision produce no
intent. The host must revalidate the revision and target again before acting;
recreate the view before revision exhaustion.

```sh
cmake -S /path/to/copied-example -B build \
  -DCMAKE_PREFIX_PATH=/path/to/nexaui-sdk \
  -DAETHERI_SOURCE_DIR=/path/to/aetheri-snapshot
cmake --build build
./build/aetheri_desktop_view_test /path/to/font.ttf
```

This is an initial dock and model-result view, not a complete shell adapter.
Search edits return bounded whole-query proposals; the host applies or rejects
them and advances its model revision. On a changed revision the query field is
read-only for that frame, so queued older edits do not alter its published text
or produce an intent; it becomes editable again for the next frame. That frame's
semantic snapshot also reports read-only. Up/Down in the search field proposes
a selected result to the host; Enter proposes activation. The model-confirmed row
is highlighted, exposed as a selected list item and revealed after a model
revision. Native selection-owner actions and independent assistive-client
verification remain open, as do Aetheri's Tab behavior, real window activation, native
input and IME, borrowed-surface presentation,
AT-SPI publication, scaling, multi-output ownership and the original process
owner integration remain open. The test uses synthetic context events and
semantic requests; it does not establish actual Aetheri or human acceptance.

On 5 November 2026 the initial source `3c0dfd20cb7217cbbe81d0af7965533c19b0f506`
was built on Alpine/WSL2 with GCC 15.2 against an installed static, Wayland-only
SDK built from `a7a3df4b8f803cc15f876b4623c9008834c590e9`; the test passed with
DejaVu Sans. A separate static Clang 22.1.3 ASan/UBSan build, including the
consumer and model, also passed with leak detection. The model snapshot's Git
blob IDs were `be57398f32c1e16315510bd95c743559b60c7b52` for `model.c` and
`5a7ed35e92744e5a4b08254b26151efc2025cb34` for `model.h`, identical to those
files in Aetheri commit `78932584c30f4364814d227e434feeb2b74078ed` at inspection.
The older installed SDK encountered first was version 0.1.3 and was correctly
rejected by the example's 0.1.5 minimum; it was not counted as a pass.

Source `b645fee6cd0e5743bb88e22b8eb24ed9dc2922b2` adds scrollable model
results and distinguishes fixed app tags from observed window IDs. The same
GCC and Clang ASan/UBSan consumer builds passed a window-activation test and
a remove/re-add case where a queued request for the old ID is dropped after
the model revision changes. A separate test-only copy with the revision check
removed failed that case. This does not protect a caller that fails to advance
or recheck its revision, and it does not establish real compositor activation.
