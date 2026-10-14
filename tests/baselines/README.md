# Headless widget integration baselines

`nexaui_visual_regression` renders a 180 × 140 logical-pixel scene with a
layout-arranged panel, button, checkbox, toggle and slider. It runs pointer
activation across frames, checks the resulting semantic states, then compares
every opaque RGB pixel with these P6 PPM files. The test also changes one pixel
in memory and verifies that the comparison rejects it. It checks that the
unchecked checkbox is distinguishable from the adjacent panel surface.

The six images cover the light, dark and high-contrast theme presets before
and after activation. The scene uses no system font or native renderer. These
baselines were generated and visually reviewed on Windows with MSVC. The
headless renderer is the reference; these files do not validate native GPU
output, text rendering, accessibility services or other operating systems.

Run the comparison with `ctest --test-dir build -C Debug -R
nexaui_visual_regression --output-on-failure`. To regenerate after an intended
visual change, build `nexaui_visual_regression_tests`, run the executable with
`--update-baselines`, inspect all six images, then run the comparison again.
Baseline changes should be reviewed alongside the code change that caused
them.

`nexaui_plot` has three additional 220 × 140 PPM baselines:
`plot_dark`, `plot_light` and `plot_high_contrast`. They show one spiky series,
axes and a grid without a system font. `test_plot.c` also checks pointer and
keyboard cursor movement, semantic state, disabled behavior and retry after
allocation failure. The images were generated and visually reviewed on
Windows with MSVC. Run `ctest --test-dir build -C Debug -R '^nexaui_plot$'
--output-on-failure`; regenerate through `nexaui_plot_tests
--update-baselines` only after reviewing the changed output.

`nexaui_telemetry` has three 400 × 230 theme baselines with metric and
profiler bars, including warning and critical levels. Its test checks semantic
names, status and timing values, empty states, rollback and 10,000 updates over
100,000 logical records. These baseline scenes omit fonts for portability.
On Windows, `nexaui_telemetry_tests --font-preview
C:\Windows\Fonts\arial.ttf build\telemetry_font.ppm` and
`--font-preview-large` rendered normal and 200% text for visual review. Those
manual previews are not portable golden files. Native text, GPU and
screen-reader validation remain separate.
