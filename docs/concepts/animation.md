# Animation and reduced motion

`nx_transition` is caller-owned scalar state. Zero-initialize it to start at the first target, or call `nx_transition_reset` to choose a starting value. Call `nx_transition_update` while building each frame that shows the effect. It advances by that frame's `delta_seconds` at most once, even if a widget asks for the value repeatedly. Values progress linearly or with smoothstep easing. A changed target, duration or easing restarts an active transition from the value currently on screen. `nx_transition_reset` also cancels an active transition.

The animation value controls presentation only. Apply input and application state immediately. Keep names, values, focus and other accessibility semantics tied to the application state so they remain useful while the visual transition runs or when it is skipped. A zero duration snaps to the target. The caller may omit subsequent updates to pause an effect; it must keep calling while `out_active` is true to finish it. Call `nx_transition_reset` before reusing a transition with another context.

`nx_context_set_reduced_motion` accepts 0 or 1 between frames. When enabled, the next transition update snaps to its requested target and reports inactive. It does not delay input or semantic updates. The preference is per context and defaults to 0. On Windows, `nx_window_get_reduced_motion` reads the [client-area animation setting](https://learn.microsoft.com/en-us/windows/win32/winauto/client-area-animation) through `SPI_GETCLIENTAREAANIMATION`; an application can query it before building a frame and pass it to its context. The query returns an error if Windows cannot read the setting. Other native backends have not implemented the query yet.

```c
/* Initialize fade once and keep it with the view's other state. */
nx_transition fade = {0};

/* Repeat the following block for every frame. */
int reduced = 0;
if (nx_window_get_reduced_motion(window, &reduced) == NX_OK)
    nx_context_set_reduced_motion(context, reduced);

nx_begin_frame(context, width, height, delta_seconds);
double opacity = 0;
int animating = 0;
nx_transition_update(context, &fade, visible ? 1.0 : 0.0,
                     0.20, NX_EASING_SMOOTHSTEP, &opacity, &animating);
/* Use opacity for paint; expose visible and its meaning in semantics now. */
nx_end_frame(context);
```

For a built-in toggle, set `nx_control_desc.switch_transition` to a persistent transition and choose `switch_duration_seconds` and `switch_easing` (zero easing selects smoothstep). Only the thumb position animates. The checked model, semantic state and focus feedback update in the activation frame. The transition is restored if painting fails, so the control can be retried in that frame. Leave the pointer null for an immediate thumb position.

The duration in this example is an application choice. Reduced motion is verified in headless transition and semantic tests. Native preference detection is tested on Windows; propagation to a running application and assistive-technology behavior still require native integration acceptance.
