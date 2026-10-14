# Cards, actions, icons and cached text

These are ordinary NexaUI components, independent of the NexaCloud example,
operating system and renderer. Their output uses existing shape, bitmap and
clip commands. See [the NexaCloud example](../../examples/nexacloud/README.md)
for composition into a dashboard and a file browser.

`NX_THEME_MIDNIGHT` adds deep blue surfaces and cyan accents to the existing
Dark, Light and High Contrast presets. Applications still resolve partial
overrides and apply a base theme between frames. The three-choice theme
selector retains its original three choices; Midnight is selected explicitly.

`nx_draw_surface` paints a rounded surface with an inside rounded border.
`nx_card` uses the current surface, border and radius tokens and registers
PANEL semantics. It creates no implicit scope. Register children using the
card's ID and use a scroll area or explicit clip where needed. Surfaces paint
the border first and the inset background second; translucent paints compose
in that order.

`nx_action` combines a named, focusable hit region with optional vector icon,
label and selection. Primary, Secondary, Quiet, Navigation and Tile variants
use the current theme, including hover, held, disabled and focused states.
Navigation exposes TAB/SELECT and tiles LIST_ITEM/SELECT. Other actions expose
BUTTON/PRESS. Selection belongs to the application: inspect `state.clicked`,
update your model and supply the new state in subsequent composition. Tab and
Shift+Tab follow registration order; Enter and Space activate. Composite lists
add their own directional navigation and scroll-to-focus policy.

```c
nx_action_desc item = {0};
item.control.id = 42;
item.control.parent_id = sidebar_id;
item.control.bounds = (nx_rect){12, 120, 208, 48};
item.control.name = (nx_utf8_span){"Files", 5};
item.variant = NX_ACTION_NAVIGATION;
item.icon = NX_ICON_FOLDER;
item.icon_bounds = (nx_rect){26, 132, 24, 24};
item.selected = current_page == PAGE_FILES;

nx_interaction_state interaction;
nx_result result = nx_action(ui, &item, &interaction);
if (result == NX_OK && interaction.clicked) current_page = PAGE_FILES;
```

Visible text is optional; an accessible name is always required. Supply either
`control.label_layout` and a positive glyph-cache budget, or `label_stamp`.
Both use `control.label_origin`. A tile can contain an `nx_image`, labels and
other child display nodes. Interactive children need their own stable IDs.
Keep file models and thumbnail ownership in the application.

`nx_progress` displays a finite fraction in `[0,1]` with PROGRESS/READONLY
semantics and a copied percentage value. It does not animate or perform work.
Use application text to identify upload/download, status and remaining time.

`nx_draw_icon` supplies original, portable line icons for navigation, files,
status and common actions. It requires no symbol font or platform assets.
Icons are decoration; use a named widget for an icon-only action. NONE draws
nothing. The icon, surface, card, progress and action operations roll back
their appended commands on failure; widgets also roll back semantics and hit
regions. Retrying an action after allocation failure does not leave a duplicate
ID behind.

## Text stamps

`nx_text_stamp_create` rasterizes a monochrome text layout once into an owned
A8 bitmap and copies the layout's source text. After successful creation,
the layout, font collection, font and font bytes can be released. The allocator's
user state must remain valid through stamp destruction. Creation has explicit
dimension/pixel limits and returns a null output on failure.

`nx_draw_text_stamp` appends one bitmap command; `nx_label_stamp` also registers
the copied text as a semantic label. An action's optional `label_stamp` uses
the same path. A blank stamp has no pixels and draws no command, but still
requires an active frame. Stamps are immutable and can be reused with different
colors. Keep each stamp alive while a renderer can still read its pixels.

A stamp caches a particular raster size and phase. Recreate it when the desired
font size changes. Integer logical origins preserve its baked sampling phase;
the reference scene snaps cached label origins to whole logical pixels.
As with other bitmaps, scaled drawing uses nearest-neighbor
sampling. It is intended for repeated labels, rather than replacing text layout,
editing, rich text or resolution-aware font selection. Combining monochrome
coverage before tinting can differ from separately tinting overlapping glyphs;
the stamp contract does not promise byte equality with per-glyph compositing
for every color and overlap. The unit test checks white-ink output, font-release
lifetime, blank output and every creation allocation failure.
