# Design A framework acceptance and integration handoff

Review date: 2026-10-18. Framework source:
`33dcf12c53e1f609f40b5b0e43fe6f273ed77a3f`.
This consolidates the four delivered framework requests. It does not accept the
complete NexaCloud application, every operating system, or a release gate.
The public target platforms are Windows, macOS, Linux X11 and Wayland.
The subsequent [widget text-density follow-up](context-text-density-report.md)
adds a context default for editable/rich/internal text paths and records the
Metal clip repair exposed by the expanded native fixture. Its exact source and
validation are separate from the original consolidation below.

## Delivery and executed evidence

| Request | Delivered contract | Executed evidence | Remaining acceptance |
| --- | --- | --- | --- |
| A01 rounded gradients | `nx_surface_image_create`, solid/linear/radial RGBA; nested rectangular clips | Endpoint/degenerate/premultiplied-alpha/radius checks, allocation rollback and four densities; byte-exact D3D12 hardware/WARP, Vulkan and Metal fixtures | Whole-app visual composition, physical displays, arbitrary-ratio image quality |
| A02 soft shadows/glow | `nx_shadow_image_create`, immutable padded mask; caller-controlled cache | Soft alpha levels, support/padding, four allocation failures/retry; common-density native fixture; paired Release construction/draw measurements | App cache cadence, actual upload/Present/input latency, physical contrast review |
| A03 protection icons | Shield, ShieldCheck and WiFiShield appended to existing icon set | Four theme presets and 16/24/32/48 logical sizes; common-density native fixture | Final parent names/status and human assistive tasks; the icon establishes no network security |
| A04 multiple filled series | `nx_plot_multi`, named layers/units, explicit gap segments, shared axes, persistent cursor | Three-series positive/negative/constant/gap cases, pointer/keys, memory rollback/retry and bounds; native bitmap parity; new fallback/native UIA tests below | App legend/tick typography, per-series accessible readings/announcements, real rates, full assistive task |

The [original report](design-a-framework-report.md),
[D3D12 sampling repair](d3d12-texel-identity-report.md),
[paired CPU profile](paint-cost-report.md) and
[density-aware text evidence](text-density-report.md) retain their own exact
revisions and limits. The old red D3D12 run remains historical failure evidence;
no tolerance was widened to close it. Paint construction/fill optimizations in
`18345c0`/`5004083` preserve bytewise output. The subsequent `b5946b4` text change
has seven successful required desktop jobs recorded in [its receipt](text-density-ci.json):
Windows 105/105 and macOS 91/91 per static/shared variant; Ubuntu and sanitizers
105 executed passes plus seven unavailable-Wayland skips each.
Those results are historical exact-source evidence, not results for `33dcf12`.

## New fallback and native-value checks

`tests/design_a_fallback_scene.h` is a private geometry/semantics fixture, with
cached gradient/shadow resources, named WLAN sample status, two checkboxes and
two named plot layers. It intentionally has no font labels and is not a final
application screen. Normal mode records four RGBA images (card, shadow, two plot
masks). High contrast or reduced effects selects opaque theme surfaces and line
plots, recording zero RGBA images. High-contrast custom lines use theme text and
accent colors; `hide_fills` alone does not recolor application-specified lines.
Theme changes apply at the next completed frame, with stable semantic IDs.

The portable test executes 48 normal/contrast/effects/density frames at
1/1.25/1.5/2×, plus action transitions. It checks both line colors in actual
software pixels, the focus stroke, advancing keyboard cursor, semantic status,
names/units and mask removal/restoration. Nominal opaque text/surface colors
must exceed 4.5:1 and focus/HC line colors 3:1 without rounding. These are bounded
technical checks, guided by [W3C's contrast guidance](https://www.w3.org/WAI/WCAG22/Understanding/non-text-contrast.html),
not full desktop accessibility certification or checks of readable labels.
An isolated mutation forcing effects on fails the mask-count assertion; the
unchanged fixture passes with ASan/UBSan/LSan. Both final runs release resources.

The independent Windows client found a real transport defect: the core cursor
advanced to `x=0.0176056`, while UIAutomationCore still returned an empty value.
The simple provider had a Value property branch but no ValuePattern provider.
`33dcf12` implements read-only `IValueProvider` for unprotected `NX_ROLE_CUSTOM`
nodes without SET_VALUE capability. This follows Microsoft's
[ValuePattern contract](https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-implementingvalue).
It adds no public C API and no editable-text contract.

The native probe checks actual OS names, roles, focus capability, HelpText units,
changing ValuePattern/property reads, rejected SetValue, UIA checkbox toggles,
and keys posted to its own HWND in normal and fallback modes. Held pattern calls
are checked after protection/capability changes and removal. Empty normalized
reads are accepted only with rejected mutation and no replacement value.
Native foreground focus is compared with the actual OS result; logical focus
alone is not reported as physical keyboard focus. The host does not present this
fixture. This is native transport/input automation, not a Narrator task.

Local commands, source/log hashes, the negative probe, display inventory and
final outcomes are in [the local receipt](design-a-acceptance-local.json).
All seven required jobs of [exact-source CI run 39084241953](https://github.com/Lulus792/NexaUI/actions/runs/39084241953)
pass: Windows static/shared each 107/107, macOS each 92/92, Ubuntu static/shared
and sanitizers each 106 executed passes plus seven unavailable-Wayland skips
(113 registered). The new portable fallback executes on all three OS families;
the new native UIA case and existing three host models execute on both Windows
variants. Existing D3D12 hardware/WARP, Metal, Vulkan, native accessibility and
package-consumer checks also pass. Optional Windows ASan/fuzz CI jobs are skipped;
local instrumented MSVC ASan passes both new cases. See the
[CI receipt](design-a-acceptance-ci.json) for per-job counts, log hashes and steps.

## Ownership, bounds and fallback policy

Create replacement paints outside frame building; key caches by size, density
and style and bound their aggregate memory. Constructors borrow stops only for
the call and leave the output pointer unchanged on failure. Keep the old image
or draw a plain theme surface, then retry. Paint images own pixels until destroy;
draw commands borrow them through renderer consumption. Never destroy a cache
entry merely because `nx_end_frame` returned. Native private drivers stage/copy
pixels before submit returns; this does not permit freeing before submit.

Paints cap each physical axis at 4096 and total pixels at 4,194,304, density
at .125–8, blur radius at 64 physical pixels per pass and caller construction
peak budget at 64 MiB. Shadows use three box passes and two temporary float masks.
Stored pixels cost four bytes each. These ceilings are not performance targets.
The measured large shadow retains 4,096,480 bytes, peaks at 12,289,280 bytes and
takes roughly 50–63 ms in warmed local Release construction measurements. Cache
it outside frames; CPU timing includes neither upload nor Present.

Plots borrow samples during the call; keep caller scratch/state reusable.
Filled masks are context-owned until next begin-frame/destruction, with up to
eight layers and 4,194,304 total filled pixels. The measured two-layer frame still
allocates two masks. Span tables add 16 KiB bounded stack per paint call; full
mask zeroing remains area-dependent. `hide_fills`, opaque surfaces and omitted
glow are functional fallbacks, not a guarantee that every custom color contrasts.

Use real Regular/Semibold/Bold font collections in NexaCloud and retain cached
text stamps through rendering. Density-aware stamps preserve logical layout;
their physical masks do not replace physical DPI/monitor review. See the
[paint](../concepts/paints.md), [plot](../concepts/plotting.md),
[text](../concepts/text.md) and [accessibility](../concepts/accessibility.md) contracts.

## Remaining task matrix and responsibility

| Check | Actual state | Next owner |
| --- | --- | --- |
| Different-density physical monitor migration, 4K/Retina | Open. Read-only Windows inventory reports one 1920×1080 display, 96-DPI hidden PMv2 window, no remote session. This is OS inventory, not physical-device certification. No global setting changed. | Device/platform acceptance |
| Reduced effects / high contrast | Portable raster + Windows native action/value checks executed. Full visible font-backed app walkthrough, OS preference changes, large text, color-only distinction and contrast over all composed surfaces remain open. | NexaCloud + platform acceptance |
| Narrator / VoiceOver / Orca tasks | [Orca follow-up](orca-focus-report.md): nine actual control/dialog announcement checkpoints pass after a missing new-control focus event was fixed. Observed 6–30 s delays and registry warnings keep responsiveness open. No human hearing/use, Narrator or VoiceOver signoff. | Assistive task review |
| Windows clipboard transfer | [Fixture isolation](clipboard-isolation-report.md) executes Unicode/newline/buffer checks and two native safety guards in static/shared/ASan locally. Human transfer between applications and the historical full-suite failure's cause remain open. | Platform and human task review |
| Final typography, spacing, navigation and whole-screen match | Open in NexaCloud; this framework fixture is not that application. | NexaCloud |
| Live local rates/AP/device state and cache cadence | Application responsibility; fixture data is explicitly sample data. | NexaCloud |
| Pin adoption | Separate application review. The local handoff reports adoption of `e698b22` with app tests; this review did not rerun those app checks or change its pin. The new candidate is `33dcf12`. | NexaCloud |

Windows display data comes from EnumDisplayMonitors/GetMonitorInfo and temporary
owned PMv2 windows. Microsoft documents that
[enumeration can include pseudo-monitors](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-enumdisplaymonitors)
and [GetDpiForWindow depends on window awareness](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getdpiforwindow).
No physical transition, reader speech, frame-latency budget or overall 1:1
acceptance is inferred from it. P1 evidence consolidation is complete; P2 has
executed partial checks and the above open tasks. The general goal stays active.
