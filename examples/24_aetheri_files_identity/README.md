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

The scene does not provide a native window, filter text field, scroll-wheel
handling, offscreen accessibility children, independent AT-SPI/human
screenreader evidence, full focus restoration after filtering, localization,
or the actual Aetheri Files UI. It has no file-opening or permission authority.
Large-list/100,000-row measurement, asynchronous operations, real service
state and platform/physical acceptance remain separate gates.
