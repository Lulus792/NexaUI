# Plot label fit and cancellation near zero

SDK `8e06b88366be0e064279db9779f571d22611046a` corrects the near-zero
scientific label visible in the [Physim export](physim-export-report.md).
Interior ticks on crossing ranges use a relative floating-point tolerance
for cancellation near zero. Tick endpoints keep their original values, and
the plot mapping, series and cursor keep their original precision.

The widget constrains label placement to its gutters. It omits a label if
its single-line layout cannot fit and clips glyph ink to the plot bounds.
Formatting otherwise retains the existing precision and process locale.

The exact new plot regression fails against installed SDK
`b881dd2c0156bdfe1196f743adf4d9177b647985`. It compares an exactly symmetric
range to a range whose endpoint differs at the last representable digits.
It also distinguishes two genuinely small positive intervals and checks
that long labels cannot paint into the canvas margin. Existing no-font
goldens remain byte-identical.

All 12 selected SDK CTests and 15 installed Physim CTests pass in static
Release, shared Release and ASan/UBSan/LSan. The unchanged Physim export
consumer `a14cfda4d8c85fa6bb07cbbce773c5412d7192fe` produces identical
802×502 RGB output in the three profiles. In the region left of the plot
at the center tick, non-background pixels fall from 20 to zero. A visual
check confirms the full zero label inside the gutter. Numeric draft
precision in the reference view remains open.

The first strict SDK build turned a pre-existing Clang 22 warning in
`tests/reference/headless_scalar.c` into an error. The accepted run uses
the SDK's usual CMake warning policy and keeps the installed consumer's
strict warnings. The test itself executes in all three profiles.

Local archive `nexaui-physim-plot350.tar.gz` contains 16,722,823 bytes with
SHA-256 `5cfb370b3c7bc332476129c0dd4b0863e3bfe1e1807ac3f727a12297b57cf764`.
The [manifest](plot-tick-label-evidence.json) binds the complete images,
SDK source, build logs and old-SDK failure. The local updated image is
`physim-plot350.png`. The [cross-platform CI](https://github.com/Lulus792/NexaUI/actions/runs/43972894597)
tracks the exact SDK source.
