# Theme contract

NexaUI exposes semantic color and metric tokens through `nx_theme`. Widgets choose tokens by meaning rather than embedding palette constants. The current tokens cover surfaces, normal/muted/disabled text, borders, accent and selection pairs, hover and pressed states, focus, validation colors, shadow color, spacing, control height, border/focus widths, corner radii, shadow geometry, font size, line height and disabled opacity. Font family selection, state transitions and a complete widget style system remain later work.

`nx_theme_preset` creates complete dark, light or high-contrast values. Each preset defines all current tokens. The background/text, surface/text, accent/accent-text and selection/selection-text pairs have a contrast ratio of at least 4.5 in the synthetic token test. The high-contrast preset uses thicker borders and focus rings, zero radii and full disabled opacity. Numeric units are logical pixels except disabled opacity, which is a fraction from zero to one.

A zero-initialized `nx_theme` is an empty override. `nx_theme_set_color` and `nx_theme_set_metric` mark individual tokens as set after validating their values. `nx_theme_resolve(parent, overrides, out)` copies inherited values and replaces only marked tokens. It supports an output that aliases either input, and rejects invalid masks or values without changing the output. Parent data and overrides are caller-owned values; resolution does not retain pointers or allocate.

Each context begins with the dark preset. `nx_context_set_theme` may replace its base between frames and merges a partial theme over dark defaults; passing `NULL` restores dark. The next frame sees that base. During frame construction, `nx_push_theme` inherits the currently resolved values and applies a partial override for a subtree. `nx_pop_theme` restores the parent values. Scopes are limited to `NX_MAX_THEME_DEPTH` and must be balanced before `nx_end_frame`. `nx_theme_color` and `nx_theme_metric` query the current resolved values, including outside a frame for the base theme.

```c
nx_theme light, panel = {0};
nx_theme_preset(NX_THEME_LIGHT, &light);
nx_context_set_theme(ctx, &light);
nx_theme_set_color(&panel, NX_COLOR_SURFACE, (nx_color){0.95f, 0.96f, 1, 1});

nx_begin_frame(ctx, width, height, elapsed);
nx_push_theme(ctx, &panel);
nx_color surface;
nx_theme_color(ctx, NX_COLOR_SURFACE, &surface);
nx_fill_rect(ctx, panel_rect, surface);
nx_pop_theme(ctx);
nx_end_frame(ctx);
```

Context theme state is confined to its UI thread, like frame and layout state. A theme change affects subsequent token queries; recorded draw commands already hold concrete colors. Widgets will resolve tokens when building each frame so runtime switches take effect without rebuilding application data.

## Color and theme controls

`nx_color` stores normalized sRGB channels. `nx_color_to_hsv` and `nx_color_from_hsv` convert between it and normalized hue, saturation and value while preserving alpha. Achromatic RGB colors convert to hue zero. `nx_color_parse_hex` accepts `#RGB`, `#RGBA`, `#RRGGBB` and `#RRGGBBAA`; missing alpha becomes opaque. `nx_color_format_hex` emits six or eight uppercase hexadecimal digits with nearest 8-bit rounding. Invalid values leave caller-owned outputs unchanged.

`nx_color_picker` paints a saturation/value field, a hue strip, an optional alpha strip and a preview swatch. Give each part a stable ID and an accessible name. Pointer capture permits dragging beyond a strip; the final position clamps to its range. Arrow keys adjust the focused part, Shift uses a larger step, and Home/End set hue or alpha endpoints. The caller owns `nx_color` and a zero-initialized `nx_color_picker_state`. The state remembers hue while saturation is zero, so choosing a hue on gray or black is retained until the user adds saturation. The widget exposes color and component values in its semantic tree and commits changes only after successful painting.

`nx_theme_selector` paints three radio choices previewing the dark, light and high-contrast presets. Click, Enter/Space, Left/Right and Home/End change a caller-owned `nx_theme_kind`; focus follows keyboard selection. Optional caller-owned fonts paint localized choice names. To apply the chosen preset, call `nx_theme_preset` and `nx_context_set_theme` **between frames**. The selector does not replace a theme during frame construction. Native assistive action dispatch and a text-based hex editor remain later work.

Built-in buttons use `NX_COLOR_ACCENT_TEXT` for their label on the normal accent fill. Hovered and pressed fills use `NX_COLOR_TEXT`, and disabled buttons use `NX_COLOR_DISABLED_TEXT`. This keeps the High Contrast preset's black label legible on its yellow accent background.

## Midnight preset

`NX_THEME_MIDNIGHT` supplies deep blue surfaces and cyan accents for interfaces
such as the [NexaCloud example](../../examples/nexacloud/README.md). It follows
the same semantic token and override contract as the three original presets.
The existing three-choice theme selector retains Dark, Light and High Contrast;
select Midnight explicitly with `nx_theme_preset`.
