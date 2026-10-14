# Initial macOS Metal backend

The private `nexaui_renderer_metal` target implements initial A01/A02 clear/present
for the [AppKit adapter](macos.md). Shapes, clipping, composition and image/glyph
upload remain A03–A05 work before native NexaCloud rendering is available.

## Build and ownership

Apple builds enable `NEXAUI_BUILD_METAL`; disable it for a core/window-only build.
The target directly links OS Metal, QuartzCore and CoreGraphics. Its build-tree
C header exposes an opaque renderer and diagnostics, with no Objective-C/Metal
types. The installed core has no direct Metal dependency.

The main-thread renderer leases an AppKit window and attaches a CAMetalLayer,
retaining its previous layer state for restoration. Competing leases and
wrong-thread calls fail. Destroying the window while leased leaves it alive.
The view receives the custom layer before enabling `wantsLayer`, following
AppKit's layer-hosting order; destruction restores the prior view state.
An owned BGRA8 UNORM canvas receives a quantized RGBA8 clear; a GPU blit copies
it to the drawable. The layer declares sRGB color space. Resize allocates the
replacement before dropping the prior canvas and checks an initial conservative
8192-pixel axis cap. Zero-size targets suspend.

## Finite retirement

The renderer retains the pending command buffer, which retains its encoded
resources. It polls status for at most one second instead of an unbounded
`waitUntilCompleted`. No asynchronous callback dereferences the C renderer.
A timeout suspends rendering; close returns `NX_ERROR_LIMIT` with resources and
the window lease intact. Retry close rather than freeing them. Terminal command
errors latch the renderer until explicit recovery, which retires work before
recreating the device/queue and canvas state.

`allowsNextDrawableTimeout=YES` bounds CAMetalLayer acquisition to its documented
one-second wait. An unavailable drawable suspends the frame. A render autorelease
pool promptly releases the caller's drawable reference after commit; Metal/Core
Animation retain their own presentation resources. GPU completion is not proof
that a frame has become visible on screen.

## Validation status

The Windows/Linux host cannot compile or execute Objective-C Metal code. Its C17
example entry point compiles as portable C; host configurations still build.
Native Apple SDK compilation and Metal execution remain pending CI/hardware.
Source and authored tests do not establish GPU success.

`nexaui_metal` is authored to compare every GPU canvas RGBA8 byte with the software
reference, including alpha, quantization ties and row padding. It checks resize,
allocation faults, drawable unavailability, thread/lease rules, retained readback,
recovery and retryable driver close. It and the example skip explicitly only if
there is no Metal device. `NEXAUI_REQUIRE_METAL=1` makes absence fail on required
hardware runs. Tests enable the Metal debug layer; actual results will be recorded
here. A skip never establishes native GPU support.

Timed-out readback keeps an owned shared buffer and leaves caller output
untouched. It never retains the caller's output pointer. After completion the
helper converts BGRA to RGBA, preserving row padding. Readback is test-only;
production does not upload a CPU-rendered frame. Display color management,
hardware performance, physical DPI/input and native NexaCloud remain open.

## Primary references

- [AppKit layer-hosting order](https://developer.apple.com/documentation/appkit/nsview/wantslayer)
- [Drawable ownership](https://developer.apple.com/documentation/quartzcore/cametallayer)
- [Bounded drawable acquisition](https://developer.apple.com/documentation/quartzcore/cametallayer/allowsnextdrawabletimeout)
- [Command buffer status](https://developer.apple.com/documentation/metal/mtlcommandbuffer)
- [Unbounded waitUntilCompleted](https://developer.apple.com/documentation/metal/mtlcommandbuffer/waituntilcompleted%28%29?language=objc)
- [Texture readback into a shared buffer](https://developer.apple.com/documentation/metal/reading-pixel-data-from-a-drawable-texture)
