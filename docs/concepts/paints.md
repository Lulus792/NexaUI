# Reusable gradients and soft forms

`nx_surface_image_create` creates an immutable raster paint for rounded cards,
backgrounds and progress fills. Solid, linear and radial paints share the same
C17 contract. `nx_draw_paint_image` records an ordinary RGBA8 bitmap command;
the existing headless, D3D12, Vulkan and Metal paths consume it. No new GUI,
vector, font or image dependency is introduced.

```c
nx_color_stop stops[] = {
    {0, {0.01f, 0.12f, 0.20f, 1}},
    {1, {0.00f, 0.03f, 0.06f, 1}}
};
nx_surface_image_desc desc = {0};
desc.width = 320; desc.height = 160; desc.radius = 12; desc.scale = 2;
desc.paint = (nx_paint){NX_PAINT_LINEAR, {0}, {0,0}, {320,160}, stops, 2};
desc.byte_budget = 8 * 1024 * 1024;
nx_paint_image *card = NULL;
nx_result result = nx_surface_image_create(&desc, NULL, &card);
/* During a frame, after checking result: */
/* nx_draw_paint_image(ui, card, (nx_point){20,20}, (nx_color){1,1,1,1}); */
/* Destroy only after renderer use ends: nx_paint_image_destroy(card); */
```

The shape starts at local (0,0). Gradient endpoints and the radial center/radius
point use local logical coordinates. Stops contain encoded sRGB colors; RGB
interpolates premultiplied with alpha, then becomes straight RGBA8. This avoids
a transparent stop introducing a colored fringe. It is not linear-light or
display-color-managed interpolation. Stops must number 2–16, increase strictly,
start at 0 and end at 1. Values outside the gradient clamp to endpoint colors.
A zero-length vector uses the final stop. Radius clamps to half the smaller
shape dimension. Four fixed samples per physical pixel provide rounded alpha
coverage. Rectangular command clips compose normally, including nested clips.

`nx_shadow_image_create` rasterizes a rounded silhouette and filters its coverage
with three separable box passes. Each pass has physical radius
`ceil(softness * scale)`; support extends by three such radii plus a transparent
pixel on every side. This is a soft-form effect, not an OS backdrop blur or a
general image convolution API. Color alpha multiplies coverage. The signed
logical offset translates the entire padded result. `nx_paint_image_view`
returns the actual relative bounds, including offset and support, so a caller
can avoid clipping the halo accidentally. Place the effect before its card.

Resources are explicit application caches: create once outside frame building,
reuse for stable geometry, then rebuild a candidate when size/density/style
changes. Constructors do not touch a frame and leave the output pointer
unchanged on failure. Keep the old resource on failure and retry later. Release
old images only after their renderer use ends. A create call borrows stops only
for that call; image views borrow pixels until destroy. There is no global cache.

Limits are 4096 physical pixels per axis, 4,194,304 pixels, scale .125–8,
64 physical blur-radius pixels and a caller peak-byte budget capped by a 64-MiB
allocation ceiling. Stored RGBA costs four bytes/pixel; shadow construction
temporarily adds two float masks. Three horizontal/vertical running-sum pairs
keep blur work linear in image pixels. No temporary mask is retained after
create. Invalid values return ARGUMENT; dimensions/budgets return LIMIT;
allocation failure returns MEMORY. The caller chooses plain theme surfaces and
omits shadows for increased contrast/reduced effects; no effect is required for
interaction or conveying status.

Bitmap sampling remains nearest-neighbor. Match resource scale to output density
and align geometry when exact pixel placement matters. Fractional D3D12 sampling
has an unresolved precision discrepancy; see the
[Design-A validation report](../quality/design-a-framework-report.md).

Shield icons append `NX_ICON_SHIELD`, `NX_ICON_SHIELD_CHECK` and
`NX_ICON_WIFI_SHIELD` without changing existing enum values. They use the same
24-unit paths and caller stroke/tint as the existing icon set. The combined
icon puts Wi-Fi in the upper left and the shield at lower right, with unchanged
logical stroke width. Give its parent control a meaningful accessible name and
connection state; an icon does not establish TLS or network security.
