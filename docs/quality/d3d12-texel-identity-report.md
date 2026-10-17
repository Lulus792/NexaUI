# D3D12 texel identity — 2026-10-17

Implementation: `876b006e24ffbe7a3da926b48a68c6f885228167`.
The previous [Design-A CI failure](design-a-framework-ci.json) is retained as
negative evidence. This change addresses the isolated 1:1 sampling cause.

## Cause demonstrated by source coordinates

The original shader always calculated `(pixel - origin) / extent * texels`,
including axes where physical destination extent equaled source texel count.
The diagnostic replaces a bitmap with an opaque coordinate-coded RGBA field:
red/green contain low x/y bytes, blue contains their high bits. The tint is white
and there is no alpha blending with a prior image. Thus a wrong coordinate is
a source-selection error, independently of card color, blur or composition.

On the unchanged renderer, the local hardware diagnostic's 1.5× shadow bitmap
selects different coordinates in 4,087 pixels. WARP selects different coordinates
in 536 pixels there, and in 582 pixels of the 2× linear card. Its complete 2×
scene reproduces the CI result: maximum channel difference 3 in 306 bytes.
The diagnostic also finds 1× errors hidden by later overpainting in the complete
scene. All observed coordinate differences are one texel in the tested fields.

These axes have equal source/destination extents. Dividing and multiplying
needlessly puts exact integer source positions through approximate float
arithmetic. The primary [Direct3D division contract](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/div--sm4---asm-)
allows direct division or multiplication by a reciprocal; its
[floating-point rules](https://learn.microsoft.com/en-us/windows/win32/direct3d11/floating-point-rules)
give reciprocal operations relaxed precision. This is not a driver-conformance
failure. A nearest-neighbor floor makes a small arithmetic error observable as
a whole source-texel change.

## Exact identity-axis contract

For a target pixel with integer index `i` and center `i + .5`, the identity
source index is `i - ceil(origin - .5)`. The shader determines that integer
bias by comparing the origin with the half-integer boundary of its floor cell.
It then adds the pixel index. The tested visible coordinate range represents
these integer and half-integer values exactly in float32. Each axis independently
uses this branch only when physical extent equals source size. Other axes retain
the normalized nearest-neighbor calculation; no float64 feature is required.
RGBA8/A8 formats and the existing integer straight-alpha composition are retained.

An intermediate direct `floor(pixel - origin)` path passed the four ordinary
densities but failed an adversarial `nextafter(-.5, 0)` source: at (4,4) it selected
texel 5 instead of 4. The final integer-bias path removes that subtraction-rounding
error. The regression clips inside the bitmap to distinguish source selection
from geometric edge coverage. It checks six origins, both formats and either or
both identity axes, for 36 cases per device invocation.

## Executed verification

- Old coordinate-coded hardware/WARP probes establish the cause before shader
  changes; the intermediate subtraction path is a separate actual negative.
- Final standalone original/coordinate fields and complete scenes are byte-exact
  on local hardware and WARP at 1×/1.5×/2×. Every comparison reports zero differing
  bytes; no tolerance was raised.
- The normal complete paint scene now requires 1×/1.25×/1.5×/2×, including the
  former fractional diagnostic. Static and shared hardware/WARP widget tests pass.
  Static D3D12 smoke/recovery and forced-WARP smoke tests also pass (4/4 targeted
  static CTests; 2/2 shared CTests). The native NexaCloud example passes separately.
- A dedicated `nexaui_d3d12_widgets_warp` CTest keeps WARP coverage mandatory on
  Windows, even where hardware is available. Native display tests are serialized.
- [Exact-source full CI](https://github.com/Lulus792/NexaUI/actions/runs/39071515081)
  passes all seven required jobs: Windows static/shared each 104/104; macOS
  static/shared each 90/90; Ubuntu static/shared and sanitizers each execute
  104 successes plus seven unavailable-Wayland skips (111 registered). Both
  Windows variants execute the ordinary and forced-WARP widget cases. Native
  Metal/Vulkan paint scenes also execute successfully. Optional Windows ASan
  and fuzz jobs are skipped. [CI receipt](d3d12-texel-identity-ci.json).

Reproduce the regular gate with `ctest --test-dir build -C Debug -R
'^nexaui_d3d12_(widgets|widgets_warp)$' --output-on-failure`. To print isolated
fields, run `nexaui_d3d12_widget_tests` with `NEXAUI_TEST_PAINT_DIAGNOSE=1`; set
`NEXAUI_TEST_FORCE_WARP=1` for WARP. The implementation's
[diagnostic source](../../tests/d3d12_paint_diagnostic.h) records its construction.
The earlier `NEXAUI_TEST_PAINT_FRACTIONAL` switch is no longer needed: fractional
scales are in the ordinary required test. `NEXAUI_TEST_PAINT_DOWNSAMPLE=1` still
separates resource density from display scale for exploratory diagnostics.

Raw local before/after logs remain under `out/d3d12-*`. Arbitrary-ratio resampling,
all float edge-coverage cases, physical monitors/driver families and release
performance remain outside this sampling proof. Full application adoption and
physical/assistive acceptance remain separate Design-A requirements.

The [local evidence receipt](d3d12-texel-identity-local.json) retains log hashes
and the actual before/after coordinate and scene observations.

## Repair protocol

The preceding three general numerical experiments were stopped and removed.
The resumed work first gathered the previously missing isolated coordinate
evidence on hardware and WARP. That demonstrated a specific unnecessary 1:1
normalization, enabling this small independently tested correction. It does not
restart an unexplained numerical-repair loop or broaden a pixel tolerance.
