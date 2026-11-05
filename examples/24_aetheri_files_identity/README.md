# Aetheri Files: held selection identity experiment

This Linux C17 consumer uses the original Aetheri Files model from immutable
revision `87ff701f7d793b825c1d9012fc7d4bbd7b30773d`; it does not modify or
replace Aetheri. Developed on NexaUI base
`358965e12779364125f28c3fa54005b0d4a24139`, it uses the installed SDK's
`nx_virtual_list` to locate a visible model row and exercises an owner-side
intent guard for a selected regular file.

The model's selected value is an index and can change after a filter or refresh.
Before a delayed action, `files_intent_stage` asks the original model for its
own held directory and `O_PATH` regular-file witness, and copies the literal
leaf. Resolution compares the current held directory and visible model entry,
reopens through the original model's checked API, compares both held
identities and the literal leaf, then restores the user's current selection.
The result consists of newly owned file descriptors and the original bytes,
not the escaped UI label. Replaced or hidden entries and other directories
return an error; an old index is never used as the action target. A hard link
with a different name is not silently substituted.

The caller supplies `query_accepted=1` only when the *entire* current query
was accepted; rejected/overflowing input must set it to zero until cleared
or corrected. Both stage and resolve reject an invalid query. The guard cannot
infer rejected text from a model that retains an earlier valid query. It also
does not authorize a file operation, protect file content against concurrent
changes, provide durable filesystem identity, replace later operation-specific
checks, or control the owner's session. FDs are borrowed only internally;
clear the intent on cancellation/close and close returned FDs. Call on the
model owner thread. An already issued operation cannot be retracted.

Build from a **separate immutable snapshot**, using an installed SDK. Include
the Unicode generated data in the snapshot:

```sh
mkdir -p /tmp/aetheri-files-snapshot
git -C /path/to/Aetheri archive 87ff701f7d793b825c1d9012fc7d4bbd7b30773d \
  src/files src/text | tar -x -C /tmp/aetheri-files-snapshot
cmake -S examples/24_aetheri_files_identity -B /tmp/nexaui-files-identity \
  -DCMAKE_PREFIX_PATH=/path/to/installed-nexaui \
  -DAETHERI_SOURCE_DIR=/tmp/aetheri-files-snapshot
cmake --build /tmp/nexaui-files-identity
/tmp/nexaui-files-identity/aetheri_files_identity_test
```

On Alpine/WSL, the installed 0.1.18 static, shared and Clang ASan/UBSan
configurations passed as an unprivileged user. The test covers filter/reorder,
invalid whole query with retained matching prefix, held directory switch,
replacement, hard-link name distinction, escaped label, descriptor ownership
and a virtual visible-range query. An index-only mutant and a mutant ignoring
query rejection both failed their intended checks. Initial setup missed the
generated `grapheme_data.h`; the final build checks it explicitly.
The tested SDK was built from the 0.1.18 candidate committed as
`00649d379c10bc4565daaf63e6faad8db275e2f2`; SDK sources and build configuration
files are unchanged between that commit and the NexaUI base above. The example
sources also compiled with C17 `-Wall -Wextra -Werror`.

That initial identity experiment is not a complete Files UI. It did not demonstrate keyboard/focus,
screenreader semantics, live updates, asynchronous operations, native Aetheri
integration, a 100,000-row workload or physical/human acceptance. The pinned
model itself limits a directory snapshot to 4,096 entries; it is not evidence
that the Aetheri model meets the separate 100,000-row toolkit performance goal.

## Virtualized scene experiment

On NexaUI base `471348c1b8fee3aa0d75eb129ba1059acc2a1c90`, the native
consumer routes list navigation only while an existing list row or its tree
owns focus, or while no control has focus. In particular, arrow and page
keys on the filter, clear or close control no longer change the file selection.
The scene test covers those controls and the active row. A private Weston/Xvfb
test sends End to the clear and close controls and verifies the model selection
after each rendered frame. This is a consumer policy, not a new SDK contract;
physical input, human screenreader review and actual Aetheri remain open.

On NexaUI base `31da772f94a2fdea863157bf06819a30aa13480b`, `scene.c` adds a
font-backed headless scene using the same pinned model, `nx_virtual_list` and
the existing tree widgets. It registers only rows intersecting the viewport,
uses full escaped labels for semantics and clips painted row text. Up/Down,
Home/End and Page Up/Down change selection and reveal it when the caller says
the list owns keyboard focus; a pointer activation selects the matching model
entry. The owner passes its events and calls `files_scene_sync` immediately
after any external model mutation, before routing or publishing another frame.

Within one held directory, the scene maps raw name, kind, device and inode to
monotonic presentation IDs, retaining a hidden selection when a filter is
removed. Directory replacement retires the directory's IDs. This mapping has
an explicit 8,192-key lifetime budget and returns `NX_ERROR_LIMIT` when full;
it is not a durable filesystem identity and inode reuse is possible. Pending
actions must still use the held-witness intent contract. Staging through the
scene rejects invalid queries, filtered-out selection and a selected row that
no longer matches after an unannounced model refresh. A failed sync blocks
further frames/actions until a successful sync. The host must not publish a
frame returned with an error.

Build the target in the same example build and pass a readable TTF:

```sh
cmake --build /tmp/nexaui-files-identity --target aetheri_files_scene_test
/tmp/nexaui-files-identity/aetheri_files_scene_test /path/to/font.ttf
```

Static, shared and Clang ASan/UBSan tests passed as an unprivileged user on
Alpine/WSL. They check bounded visible semantics, bitmap generation and
headless rendering, selected/focused roles, keyboard and pointer selection,
filter restoration, invalid queries and refresh-before-sync rejection. A
row-index ID mutant and a mutant bypassing the pre-stage recheck failed the
corresponding tests. A 240x84 headless capture showed three legible rows and
the selected-row highlight; this is a limited visual inspection, not design
acceptance. Test source and adapter also passed C17 strict warnings.

At that base, the scene did not provide a native window, filter text field, scroll-wheel
handling, offscreen accessibility children, independent AT-SPI/human
screenreader evidence, full focus restoration after filtering, localization,
or the actual Aetheri Files UI. It has no file-opening or permission authority.
Large-list/100,000-row measurement, asynchronous operations, real service
state and platform/physical acceptance remain separate gates.

## Native Wayland window experiment

On NexaUI base `ef020881b4c2b12f52590388c3405a9472802033`, the optional
`aetheri_files_native` target connects the same scene and original model to an
ordinary NexaUI window and software renderer. Up/Down, Home/End, Page Up/Down,
pointer selection and wheel scrolling operate the visible list; Ctrl+R reads a
fresh model snapshot, Ctrl+Q closes, and SIGINT/SIGTERM use the same orderly
cleanup. Frames publish window semantics and route native accessibility
requests before rebuilding. When AT-SPI is unavailable, the loop retries
publication; idle waits do not force redraws. Diagnostics containing file names
are emitted only when the test-specific `NEXAUI_FILES_NATIVE_TRACE` is set.

The native driver owns a temporary directory and private Weston/Xvfb seat. It
checks actual virtual keyboard input, pointer selection, wheel scrolling after
an observed pointer entry, refresh and clean close. An independent libatspi client with a private
registry finds the native tree rows, invokes `select`, observes the selected
state and confirms that the original model changed. Static, shared and Clang
ASan/UBSan configurations passed both tests on Alpine/WSL. Isolated mutants
that suppressed wheel movement or accessibility-request routing failed their
respective checks. The earlier single fixed pointer coordinate missed Weston's
window; the final driver searches for an observed pointer entry instead.

Build from the same pinned snapshot and installed SDK as above. Set
`NEXAUI_WESTON`, `NEXAUI_WESTON_SHELL` and `NEXAUI_WESTON_LIBEXEC` to the
existing private Weston installation, then run each command with a new work
directory as an unprivileged user:

```sh
xvfb-run -a python3 tests/run_weston_native.py --work-dir /tmp/new-files-native -- \
  python3 examples/24_aetheri_files_identity/run_native.py \
  /tmp/nexaui-files-identity/aetheri_files_native /path/to/font.ttf /path/to/xdotool
NEXAUI_FILES_FONT=/path/to/font.ttf xvfb-run -a python3 tests/run_weston_native.py \
  --work-dir /tmp/new-files-atspi -- python3 tests/run_atspi_native.py \
  /path/to/at-spi2-registryd /tmp/nexaui-files-identity/aetheri_files_atspi_client \
  /tmp/nexaui-files-identity/aetheri_files_native
```

At that base this was a read-only integration probe, not Aetheri's Files
application: it had no filter editor, directory entry/open action, file grants
or operation UI. The viewport was capped at 1,764 logical pixels; a smaller-than-row window
temporarily exposes no list. Offscreen semantics, accessible keyboard-focus
restoration, localization, real Aetheri compositor/backend integration,
additional compositors, physical input and human screenreader/design review
remain open. A test registry and virtual keyboard do not close those gates.

## Filter field and IME experiment

On NexaUI base `00bb33d72b6938d6dbfb897911387ea6e0be46b6`, the native probe
adds the existing NexaUI text field and the original model's case-sensitive
filter. Ctrl+L clears and focuses the field; its name and invalid state are
exposed through semantics. An overlong or invalid complete draft is latched as
rejected until the field is cleared. The old model query is retained, but
`files_scene_stage` refuses it while rejected. A conservative scalar-burst
guard also catches an overlong burst followed by deletion in the same frame.
Because it counts a byte upper bound, it can reject a valid near-limit
replacement; the test explicitly demonstrates this limitation. Batched
clipboard/IME/edit combinations and all possible event-ordering cases remain
unproved. The native probe has no file activation or write operation.

Private Weston virtual-keyboard tests exercised filtering, 256-byte overflow,
deletion without reopening the retained prefix, clearing, navigation and
shutdown. An independent libatspi client edited the filter, verified invalid
state and recovery, and observed the original model. A separate
`weston-simple-im` process produced apostrophe preedit without changing the
model, followed by exactly one `á` commit and one matching result. Static,
shared and Clang ASan/UBSan variants passed. A mutant disabling the burst guard
failed the same-frame overflow test. A 240x120 headless capture of the focused
field and list was visually inspected; this is not final design acceptance.

The initial virtual IME attempt showed that activating the field on Ctrl+L
key-down could admit the shortcut's later `l` release as text. The probe now
focuses after that key is released, and its separate IME test waits for the
actual text-input enter before composing. In another observed simple-IME run,
Ctrl+Q while the field was already active arrived as a `q` commit instead of
closing; the IME test therefore uses SIGTERM for the verified orderly exit.
Ctrl+Q and Ctrl+L with an already active simple IME, other providers, stale
composition/focus transitions and physical input remain open. This observation
is specific to the private virtual setup, not a claim about every IME.

To run the separate IME probe with the private Weston environment already
described, set `NEXAUI_SETXKBMAP` to a usable `setxkbmap` and run:

```sh
python3 examples/24_aetheri_files_identity/run_ime_session.py \
  /tmp/nexaui-files-identity/aetheri_files_native /path/to/font.ttf \
  /path/to/xdotool /tmp/new-files-ime
```

The native window now reserves 36 logical pixels for the filter and caps the
list viewport at 1,764 pixels. Empty/error explanations, complete paste
semantics, file actions, native Aetheri integration, offscreen accessibility,
localization and human review remain separate work.

## Bounded simple input replay

On NexaUI base `992c31f67bea9d6b4ccb857705c24f9b0218a9c3`, the probe
replays a restricted, unambiguously focused set of text and unmodified deletion
events against a copy of the public grapheme-aware text buffer. It checks each
intermediate draft and compares the result with the actual widget. A valid
same-frame replacement at the 255-byte limit is accepted; an intermediate
overflow remains rejected even if deleted before the frame ends. The scene test
also covers replacing a two-byte character and inserting then deleting a
four-byte character. The original model query remains protected while rejected.

Static, shared and ASan/UBSan scene tests passed, along with private Weston
keyboard/pointer, independent AT-SPI and simple-IME probes in each variant.
Removing the burst guard failed the transient-overflow test; forcing the old
upper-bound path failed the valid replacement test. These tests use the
original Aetheri Files model snapshot specified above, not a migrated app.
Complex input retains the prior scalar upper bound. Clipboard contents, IME,
accessibility mutations and arbitrary combined event ordering are not proved by
this replay; the earlier active-IME shortcut finding also remains open.

On base `839f80a7e27e64e7f6f2dd28dd2b8f71fe0d6b34`, a separate private
Weston protocol trace reproduced the shortcut observation: while simple-IME
was active, the client received the Control press and release, but no Q key;
the input method instead sent a `q` text commit. The IME probe now accepts
either an actual forwarded close shortcut or a text commit that changes the
filter without closing, and checks the latter against the protocol trace.
Static, shared and ASan/UBSan variants exercised the commit path. A client
cannot infer an unreceived shortcut from arbitrary committed text. Reliable
closing and shortcut handling with other providers, composing states and real
desktops remain open; this test does not claim to repair the input method.

## Close control

On base `84c3256043bc627bb8cc8c3f7b9935a4fcb5e1d1`, the native probe
adds a labeled NexaUI button alongside the filter. The scene exposes a close
request to the window owner, which performs normal cleanup. At 240x120 the
headless rendering was inspected. A scene test exercises Enter with focus on
the button. Private Weston tests exercise Tab/Enter when the key reaches the
application, a pointer click after simple-IME composition, and activation via
an independent AT-SPI client; static, shared and ASan/UBSan variants passed.

During a separate simple-IME attempt, Tab and Enter were consumed before the
client could use them to move focus and activate the button. The click and
assistive paths do not prove keyboard-only access in that configuration.
Other providers, physical input, very narrow layouts, localization and human
accessibility review remain open. This remains a standalone probe, not an
integrated Aetheri Files application.

## Empty and rejected filter states

On base `fd5174b14241b55522af22d5a4cbf3a243b11d48`, the scene reports an
empty folder, a valid filter without matches, and a rejected filter separately.
Each is a visible message and a named status semantic node. While the filter is
rejected, rows from the retained old model query are hidden and keyboard list
navigation cannot move the hidden selection; staged intents remain rejected.
Clearing the field restores the normal list. The original model retains its
last accepted query until a valid replacement is applied.

Scene tests cover all three messages and hidden-selection behavior. An
independent AT-SPI client checks the no-match and invalid status names and
roles. Private Weston keyboard/pointer, IME and AT-SPI tests passed in static,
shared and ASan/UBSan variants. A 240x120 invalid-state headless capture was
inspected. Automatic status announcements, human screenreader review, very
narrow layouts, localization and actual Aetheri integration remain open.

On base `1edde529243be3481245e7b162319a44f9d7f16d`, the independent
libatspi client additionally registered for native events. It observed an
`object:children-changed:add` on the Files tree whose child was the no-match
status, then an `object:property-change:accessible-name` on that same status
object carrying the invalid-filter message. Static, shared and ASan/UBSan
variants passed on private Weston; suppressing either observation in the
client made the corresponding assertion fail. These events make the changes
observable; they do not establish automatic announcements by a screenreader.

## Clear-filter control

On base `aa5b5cf6d1a92b4142b0ca353f1244b3a92c3d26`, a labeled button
clears the complete rejected draft and model query through the existing scene
operation, restores the list and returns scene focus to the filter. The action
is also exposed as an AT-SPI push button named `Clear filter`. A native keyboard
test uses Shift+Tab and Enter after a 256-byte overflow; an independent AT-SPI
client clears an invalid draft; and a private simple-IME test clicks the button
after an IME commit. Static, shared and ASan/UBSan variants passed. The invalid
state at 240x120 was inspected.

The IME test did not clear an active uncommitted preedit; that transition,
keyboard-only navigation when a provider consumes Tab, very narrow layouts,
localization and human assistive review remain open. The button does not grant
access to files or change the original model's authorization boundaries.

## Clear during preedit

On base `d07ff625ea726de55f66b3c3a6023954ddeaedae`, a native simple-IME
test started with a nonempty filter and an uncommitted apostrophe preedit,
clicked Clear, and then typed `a`. The previous candidate produced `á`,
revealing that the old composition remained active. The Clear handler now
explicitly disables the native IME before replacing the filter and enables it
again for the refocused field, using the existing window API. The next `a`
is literal; a separately started apostrophe-plus-`a` composition still commits
one `á`. The original previous-candidate test failed and the corrected test
passed on private Weston in static, shared and ASan/UBSan variants. Ordinary
native keyboard and independent AT-SPI tests also passed in all three.

This is a bounded text-input-v1/simple-IME observation. Other providers,
text-input-v3, stale replies in flight, errors during context reset, physical
input and human acceptance remain open; it does not prove general composition
lifetime or keyboard shortcuts when an input method consumes them.

On base `41563c9fd5aeba89146004430a5e4926c0fbfacf`, the reset moved into
the common draft-replacement helper used by the button, the Ctrl+L clear path
and valid programmatic filter replacement. Reactivation is requested only if
the filter owns scene focus. The private simple-IME test starts a second
composition and uses a test-only signal to invoke the same programmatic clear
function as Ctrl+L; a following `a` is literal. An isolated mutant that skipped
the programmatic reset reproduced the old `á` behavior. Static, shared and
ASan/UBSan scene, native keyboard, IME and independent AT-SPI cases passed.
This signal is enabled only in the explicit private IME fixture; it does not
represent a native shortcut delivered through an input method.
