# External CPU content: snapshot to viewport

This standalone installed-C17 consumer connects the existing snapshot exchange
and viewport APIs. It demonstrates CPU frame ownership for an app-owned content
slot. Its producer is a labelled deterministic test fixture. It selects no
browser/PDF/VM engine and grants no service, guest or permission authority.

```sh
cmake -S examples/38_external_content -B /tmp/content-view \
  -DCMAKE_PREFIX_PATH=/path/to/nexaui-prefix -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/content-view
ctest --test-dir /tmp/content-view --output-on-failure
```

NexaUI 0.1.33 is required. `content_surface` links only `NexaUI::NexaUI`;
the test executable additionally uses native OS threads. The example is not
added to the root SDK build or installed API.

## Integrating the frame boundary

The app owns an `nx_snapshot_exchange`, producer lifetime, wakeup channel and a
nonzero content-session generation. Create a `content_surface` with that borrowed
exchange, generation and explicit payload/staging byte budget. Use a fresh
generation after tab/session replacement, even at the same dimensions. Keep the
exchange alive until all surfaces and producer jobs using it are finished.

Inside an ordinary UI frame, draw host controls and call `content_surface_draw`
with an `nx_viewport_desc`. Logical bounds and scale use the existing viewport
rules. A successful resize records a new `(generation, resize, width, height)`
request; read it with `content_surface_get_status` and send a copy to the app's
producer once per new request. The adapter starts no thread or engine command.
Missing or stale frames return `NX_ERROR_STATE`; the viewport rolls back while
host controls remain paintable. The app can show a waiting/error placeholder.

The producer publishes one allocation containing `content_frame_header`,
exactly `height * stride` pixel bytes and optional UTF-8 text. This is an
in-process C object contract. Fields must echo the request, specify straight-alpha
`NX_BITMAP_RGBA8`, and use a nonzero sequence that increases for each changed
frame within that request. Equal sequences denote the same immutable pixels
and text and reuse the last staged content. Padded rows are supported within the declared
budget. Finish writing before `nx_snapshot_exchange_publish`; ownership transfers
only on `NX_OK`. A thread-safe disposer releases the allocation. Producer memory
and outstanding work need their own app bounds; the surface budget does not
limit arbitrary engine allocations before publication.

With damage information, supply a nonempty bounding rectangle in physical
source pixels and the strictly earlier sequence it describes. The entire
allocation still contains the complete current image. Pixels outside that
rectangle must match the stated base for the same generation and resize ID.
Set all damage fields to zero when the producer cannot make that guarantee.
The consumer copies only the rectangle if its staging buffer holds that exact
base. A missed frame or a fresh staging buffer triggers a complete copy.
Malformed rectangles are rejected before staging changes. This is a
producer-to-staging copy optimization. Presentation submits a full frame.

The acquire callback validates identity, sequence, dimensions, stride, format,
payload length and budget. It copies a new accepted frame into bounded packed
staging, then releases its snapshot borrow **before UI `nx_end_frame`**. The
renderer subsequently borrows staging through the ordinary viewport lease.
After ending and rendering the frame, call `content_surface_complete` before
reusing/destroying the surface. Do not retain the frame commands afterwards.
Completing a deliberately discarded frame is also allowed if no renderer will
use it. Repeated unchanged frames allocate and copy no new staging payload.

Resize allocation/budget failure preserves the previous request, pixels and
sequence. Transition to zero-size suspension retires staging and advances the resize ID.
Destroy returns `NX_ERROR_STATE` while a viewport lease remains active and
preserves the surface for later completion. Before destroying a published native
context, switch/detach its native semantics using the existing window contract.
Closing a view neither stops its producer nor destroys the borrowed exchange.
Late producer completion can publish safely; a replacement's fresh generation
rejects those old frames. Stop/join the producer before destroying the exchange.
Use the app's existing wakeup integration when new content arrives.

## Executed test scope

The four cases compose an ordinary host button and the content slot, comparing
complete 64x64 RGBA output against a separate reference composition. They cover
new/unchanged frames, padded stride, scale resize, suspension, byte-budget and
allocation failures, malformed metadata, decreasing sequence, active-render
close refusal and a real worker completing after view/context replacement.
They also acquire the exchange again before end_frame and retire the producer
allocation before rendering, checking the two distinct lifetime contracts.

These four cases are installed consumer/headless-render and native-thread checks.
The damage case additionally compares complete output for matching and skipped
bases. It checks that producer replacement while a render lease is live leaves
the borrowed pixels intact.
The optional fixture below adds ordinary native composition and host focus.
The input adapter below adds a bounded normalized-key/absolute-pointer channel.
IME/candidates, engine accessibility subtrees, selected-engine input/crash
recovery and reviewed GPU interop still need integration.
A selected maintained engine and its real browser/document/
guest workflows remain external engineering gates. Guest pixels cannot serve
as host admission or permission UI. No application pin or 1.0 release changes.

The [source-bound report](../../docs/quality/external-content-report.md) records
the executed Linux profile matrix, complete-image checks and rejected generation,
snapshot-borrow and retired-pixel variants. Native/engine and Windows/macOS
consumer acceptance remain separate.

## Optional native fixture

On Linux, `-DNEXAUI_CONTENT_BUILD_NATIVE_FIXTURE=ON` additionally builds an
ordinary Wayland/software-window host and a separate libatspi/X11 test peer.
The peer's X11 dependency reads only the owned Xvfb/Weston output; the host and
SDK retain their Wayland-only presentation dependencies. Default builds do not
discover or link X11 for this example.

```sh
python3 examples/38_external_content/run_native.py \
  --build-dir /tmp/content-native --font /path/to/font.ttf \
  --registry /path/to/at-spi2-registryd --work-dir /tmp/new-content-run
```

Use the repository's existing private Weston tool configuration when required.
The runner owns its Xvfb seat, compositor work directory and assistive bus.
`--case pending-close` closes while a prepared producer allocation is held;
`--case rejected-close` closes after a late old-generation frame is rejected.
The default `full` case replaces the tab, releases that late frame, admits fresh
pixels, rejects an unsupported format, recovers and requests 2x content backing.
The recovery frame changes a rectangle over the last valid frame. The fixture
checks the staged byte count and the independent peer compares every displayed
pixel, including the unaffected area. This is still a deterministic producer,
not a selected browser or guest engine.

Native controls remain reachable by keyboard. The peer independently checks
stale object/focus rejection and entering/leaving the viewport with Tab. It reads
every pixel in the 448x208 content rectangle from the private X11 carrier window
and retains PPM captures; fresh/recovered/scaled content must match distinct
fixture colors, while rejected states show the placeholder. A successful SDK
software submit alone is not treated as displayed output. The worker uses the
existing public fd-wait API for completion wakeups, carries only copied request
metadata, and is stopped/joined before exchange destruction. Closing a tab does
not stop or replay its producer. The fixture closes its window with a held job
by releasing only its own worker gate during cleanup.

These frame cases cover ordinary native composition and host focus, not engine input
forwarding, IME/candidates, engine
accessibility subtrees, GPU import, physical output or final product design.
The [native source-bound report](../../docs/quality/external-content-native-report.md)
records all profiles, actual pixel captures, close/disposal counts and rejected
generation/display variants, including the earlier ineffective control.

## Explicit content input and host release

`input.h`/`input.c` are an app-owned example adapter over existing `nx_event`
values, separate from pixel staging. They add no installed SDK API, window
grab, worker or engine dependency. Create one gate with a nonzero session
generation, a configured escape key/modifier mask and an atomic batch sink.
The fixture uses **Ctrl+Shift+F12**, displayed before activation; product and
platform shortcut selection remains the application's responsibility.

Feed every normalized native event once, in order, including while inactive.
Queue only the returned host-event copy when `route_host` is true. This copy
removes modifiers still draining from a released content session. Do not route
the original event again. The native fixture drains `nx_window_next_event`,
queues host events with `nx_context_queue_event`, then uses
`nx_window_route_events` for pending native accessibility actions. This retains
the ordinary native-events-before-AX ordering. After each host frame/action,
synchronize actual content focus, availability and geometry with `set_target`
before polling more input. A failed enqueue is terminal for this fixture;
there is no fabricated successful route or retry of an already consumed event.

Activation requires explicit host intent, content and native-window focus,
an available target and all observed keys/buttons released. It sends FocusIn
with source-pixel dimensions. A queued Enter down cannot activate content and
leak its later up into the receiver. Focus regain or a newly arrived frame
never activates automatically. Dimensions are capped at 2^24 so integer pixel
coordinates remain exact in `nx_event` floats. Pointer coordinates are scaled
from logical half-open bounds, floored and clamped to source pixels. Wheel
deltas keep NexaUI's abstract scroll units. Leaving bounds releases input;
this absolute-pointer example does not promise relative mouse/OS capture.

Only fresh normalized key/button edges, pointer movement and wheel events
reach the sink. Duplicate downs, repeats, consumed-key downs and unpaired ups
do not create receiver actions. Native text/IME events are consumed while
active, so a physical key and its native text do not enter the guest twice.
This fixture does not enable native IME. It is **not** a text-input or full
guest-keyboard implementation: engine scancodes/keymaps, left/right modifiers,
IME/preedit/candidate geometry and unsupported keys/touch need a reviewed engine
contract. Unsupported physical keys/touch revoke with an explicit error.

Escape, host-focus loss, native focus loss, pointer cancellation, target
unavailability, geometry change and explicit release/close send one batch of
all locally admitted key/button ups followed by FocusOut under the old
generation. Later real ups/repeats drain without becoming host activations or
duplicate receiver releases. Native focus loss clears physical observations
because ups may never arrive; post-focus repeats cannot invent guest presses.
Historical pixels may remain visible after input disconnect, with a truthful
label and a disabled Enter control. They do not imply a live source.

The callback must admit the entire batch or none, copy/apply it before return,
and never retain or reenter a mutating call. **NX_OK proves only local admission,
not remote application or remote reset.** A callback failure deactivates and
fences the gate, preserving its uncertain accepted-hold ledger and error. No
automatic reset retry, replay or recapture occurs. Close frees even on release
failure and returns the uncertainty to the owner. Rebind requires an inactive
gate, a strictly newer generation and the owner's independent retirement/reset
of the old backend; it is an explicit assertion, never a remote acknowledgement.
Rebind preserves draining physical input and requires a new host target/intent.
The object uses one bounded allocation and one owner thread throughout.

The five input CTests use a separate atomic receiver model and check actual
admitted events, stuck holds, scaling, escape isolation, host draining, all
revocation causes, identities, invalid/unsupported inputs, reentrancy and sink
failure. The optional `--case input` additionally runs the complete frame case,
then real seat keys/buttons, 2x coordinates, the configured escape, native focus
loss and accessible Disconnect. Its receiver is explicitly a local test model,
not a VM/backend. Native Windows/macOS, real engines, guest applied/reset
acknowledgements, text/IME, sharing/reset authorities, engine accessibility and
physical/human acceptance remain open.

## Optional PNG producer

Configure the standalone consumer with `-DNEXAUI_CONTENT_BUILD_PNG=ON` to
build `content_png` against libpng. The producer accepts borrowed PNG bytes
and a copied `content_request`. It returns an owned straight-alpha RGBA8
snapshot ready for the existing exchange. Let the producer thread dispose
of a rejected result or transfer a successful snapshot with its disposer.
The viewport retains its existing lease and generation/resize checks.

The adapter fits the source into the requested physical viewport with
nearest-neighbor sampling and transparent margins. Encoded input, decoded
source pixels and output pixels each have a 4 MiB limit. Source dimensions
are capped at 2048 per axis. These limits describe adapter-owned buffers.
The decoder manages its own internal working memory.

With the PNG producer and native fixture enabled, `run_native.py --case png` runs the
ordinary Wayland fixture with real locally generated PNG inputs. The runner
adds a truncated PNG to exercise the decode-error path. The host reports the
failure through its accessible status and leaves the last valid image
visible. A valid follow-up frame restores progress. The fixture also tests a
late frame across tab replacement and decoding for 2× backing.

### PNG image viewer

The PNG fixture exposes the decoded image's dimensions and clipped bounds
as an AT-SPI image. The visible size label and the semantic image describe
the same accepted frame. A decode failure keeps the displayed dimensions
and image available until a valid replacement arrives.

PNG and PDF share the zoom and pan controls, including arrow-key pan
with the viewport focused and Ctrl+0 to fit. An axis that already fits
stays centered. On a scrollable axis, the image edges limit pan.
While zoomed, the PNG view samples the original decoded raster through
the fitted rectangle and the viewport clip. The original is bounded by
the same 4 MiB decode limit and is tied to the accepted frame's generation,
resize and sequence. Fit image uses the fitted raster.

Use `--case png --png-viewer` to check a landscape image, a truncated PNG,
and a portrait with fine stripes and straight alpha at both backing sizes.

### Image gallery

The host can supply an ordered collection through `gallery.txt` in its
selected directory. The file contains one distinct UTF-8 filename per
line, with up to 256 regular files. The loader pins the admitted file
descriptors so the worker uses the same entries for previews and viewing.
It rejects a file whose observed size or timestamps changed since
admission when that entry is decoded again.

**Gallery** shows three previews at a time in the host's order. Tab reaches
the image cards and group controls. Enter opens the focused image. Escape
returns focus to **Gallery**. The cards expose full filenames, selected
state and preview availability through AT-SPI. A decode failure leaves
the last accepted image visible and the gallery available for a new choice.
The same group reuses its cache until the group, collection or raster
density changes.

Use `--case png --png-gallery` to run the seven-file native workflow.

Enable `-DNEXAUI_CONTENT_BUILD_JPEG=ON` alongside
`-DNEXAUI_CONTENT_BUILD_PNG=ON` to support ordinary 8-bit lossy JPEGs in
the same manifest. This optional path requires TurboJPEG 3.0.2 or newer
and libexif. It identifies JPEG by the encoded signature and applies
EXIF orientation to the preview, fitted view and zoom raster. The size
label and AT-SPI image report the oriented original dimensions.

The input file limit remains 4 MiB. The decoder accepts up to 64 Mi
source pixels and chooses the largest supported downscale that fits the
shared 2048-pixel edge and 4 MiB RGBA budgets. Zoom uses that bounded
raster. Use `--case png --png-gallery --jpeg-gallery` to check a mixed
collection with a truncated JPEG and an EXIF-rotated progressive photo.

## Optional native browser frame workflow

Build with `NEXAUI_CONTENT_BUILD_NATIVE_FIXTURE=ON`,
`NEXAUI_CONTENT_BUILD_PNG=ON` and `NEXAUI_CONTENT_BUILD_BROWSER_FIXTURE=ON`.
Run the native harness with `--case browser --browser-engine /absolute/path/to/chromium-headless-shell`
in addition to its build, font, registry and work-directory arguments.
The fixture starts Chromium as an unprivileged process with a temporary
profile. It supplies its own offline HTML and controls Chromium through
inherited DevTools descriptors. The harness removes the profile and joins
the engine when it exits.

The UI worker sends the current surface generation, resize identity and
physical dimensions over an app-owned Unix socket. A bounded request reads
one PNG, checks its decoded dimensions against the request and publishes one
complete CPU snapshot through the existing exchange. The UI thread owns the
viewport and frame lease. The workflow checks an old tab publication at
replacement, browser process failure and restart, 1× and 2× backing, host
keyboard and AT-SPI controls, and close.

Content input, candidate geometry, browser accessibility subtree mapping,
regular website navigation and GPU buffer ownership remain separate work
for the engine integration. The browser executable is a fixture input.

## Optional PDF producer on Linux

Configure with `-DNEXAUI_CONTENT_BUILD_PNG=ON` and
`-DNEXAUI_CONTENT_BUILD_PDF=ON` to build `content_pdf`. The app supplies an
absolute path to a trusted `pdftoppm` executable, a regular PDF file and a
one-based page number. `content_pdf_frame` opens the file and gives the
descriptor to a separate Poppler process. It converts the page image through
the same PNG snapshot path as other CPU content. Call it from the content
worker. The UI uses the existing resize/generation and render-lease checks.

The adapter admits PDF files up to 16 MiB. It requests at most 1024 pixels on
the longest side from the child and caps its PNG output at 4 MiB. The worker
allows three seconds for the child to produce output and exit. It kills and
reaps the child on timeout or overflow. These limits cover this adapter's
I/O. Production policy for operating-system isolation and PDF admission
belongs to the application.

Enable the native fixture as well and use `run_native.py --case pdf
--pdf-renderer /path/to/pdftoppm` with the usual build, font, registry and
work-directory arguments. The fixture walks a two-page PDF, a replacement
with a late frame, a decode error, a change to 2× backing and navigation
back to page one. The accessible host controls expose the current page and
error status. The same page snapshot can carry up to 32 KiB of read-only
UTF-8 text from `pdftotext`. The native runner uses the reader next to
`pdftoppm` by default and accepts an explicit `--pdf-text-reader` path.

`content_pdf_frame_with_text` requests image and text from the same open,
immutable source file and page. The worker rewinds the descriptor between
tool calls. Each child uses bounded output and a three-second deadline.
An additional `pdftotext -tsv` call supplies up to 1024 word boxes. The
worker accepts a complete match to the plain UTF-8 text in order and maps
the boxes through the fitted page image, including its margins. Text remains
available if the geometry match fails. The surface stages image, text and
boxes together.

The PDF fixture uses the SDK's read-only snapshot presentation support
introduced in revision `85f5d1c`. The native peer queries the accepted text,
word extents and point offsets through AT-SPI at 1× and 2× backing. It
observes retirement of old text on page or resize changes. The scalars of
each word share its box. Spaces and line breaks have no box. Textless pages
use the image and host controls.

The fixture's **Find in PDF** field uses the existing NexaUI text editor,
window IME and clipboard. Ctrl+F focuses it. The ordinary Wayland fixture
uses Escape to return to the page. Only committed search text participates
in case-sensitive, non-overlapping UTF-8 matching, with a 128-byte query
limit. The draft stays in the field across page and backing-scale changes.

The worker retains one descriptor for the current immutable document.
After the first successful page publication it reads the document's text
once with Poppler. The index is bounded to 256 pages and 512 KiB of UTF-8,
with the same three-second child deadline. The worker hands the immutable
index to the UI, which accepts it for the matching document generation.
The viewer displays counts for the current page and the whole document.
**Next hit** opens a different matching page and wraps at the document
end. The accepted page highlights its matching word boxes when available.
Query or page changes reuse the index without invoking another text
extraction for the search. If the index is unavailable, the accepted
page can still be searched.
Add `--pdf-compose` to run the same fixture with Weston simple-IME and
Compose on its private Xvfb seat. This mode needs `setxkbmap` (or
`NEXAUI_SETXKBMAP`) and a Weston helper directory containing
`weston-simple-im` and `weston-desktop-shell`.
Normalized and case-insensitive matching and character-level geometry
remain viewer work.

### Read-only PDF text selection

**Select text** focuses the accepted page's text. Drag over words, use
Shift with Left/Right or Home/End, or select the page with Ctrl+A.
Dragging selects whole words. Keyboard movement follows grapheme
boundaries, and AT-SPI uses Unicode scalar offsets. The highlight covers
intersecting word boxes when the page supplies geometry.

Ctrl+C and the native accessibility copy action write the exact selected
UTF-8 bytes to the window clipboard. The viewer reports the copy result.
The page text remains read-only. Links stay disabled during selection.
A new accepted page clears the range and retires the old text object.
Escape from the text or **Done** returns to normal page interaction.

Add `--pdf-select --clipboard-reader /path/to/wl-paste` to the native PDF
fixture to exercise the two copy paths with a separate Wayland client.
With `--pdf-link-reader` the same run also verifies that selecting text
prevents link activation.

### PDF links

Enable `-DNEXAUI_CONTENT_BUILD_PDF_LINKS=ON` with the PDF producer to build
the separate `content_pdf_link_reader` executable. This executable needs
Poppler-GLib 21.12 or later and the corresponding development headers.
Pass its absolute path with `run_native.py --case pdf --pdf-link-reader`
using the usual PDF fixture arguments. Poppler-GLib links only into that
child executable.

The worker hands the child the pinned file and page number. The reader
returns up to 32 link annotations with direct or named page targets or
absolute HTTP(S) addresses. The web addresses use printable ASCII with
at most 256 bytes and a host without embedded credentials.
The renderer uses the crop box in this mode. The worker validates the
rotated crop-box coordinates against the fitted page image before appending
the links to the owned frame. A failed or malformed reply leaves the
image and text available. The child uses the existing bounded pipe and
three-second deadline.

The viewer exposes accepted link rectangles as buttons. Tab and Enter,
a pointer click, and the accessible Click action follow an in-document
link or show the full address of a web link. The dialog can copy the
address and can hand it to the viewer's optional host-selected launcher
on **Open link**. The worker calls that launcher with one URL argument
and a three-second deadline. The launcher must return after processing
the handoff.

Link identities change with the accepted frame. Navigation and selection
mode disable link activation. Replacing a page discards its address
preview, and Escape closes the preview. Position and zoom destinations
within a page and product browser policy remain app work.

Use `--pdf-external --clipboard-reader /path/to/wl-paste` together with
`--pdf-link-reader` to test the address, clipboard and explicit handoff.
The native fixture uses a local receiver with a reserved example address.

### PDF zoom and pan

The PDF toolbar exposes **Zoom out**, **Fit page** and **Zoom in** for
100–400% magnification in 50-point steps. The fixture shows the controls
at widths of 480 logical pixels and above while text selection is inactive.
Ctrl+wheel zooms around the pointer. Scroll, middle-button drag and arrow
keys with the page image focused pan within the page. Ctrl+0 with the
page image focused restores Fit page.

The viewer immediately magnifies the accepted raster using the existing
software presenter and viewport clip. When the zoomed page needs more
physical pixels, the existing worker renders a sharper page from the pinned
file. One extra raster request may be in flight, with a requested long edge
of at most 1024 pixels and a decoded RGBA8 limit of 4 MiB. Page navigation
takes precedence over queued refinement work. The viewer uses the last
usable raster while the worker runs or if a refinement fails. Panning
reuses it, and Fit page releases it.

The refinement uses the base raster's fitted page rectangle, including its
placement within a letterboxed viewport. The same transform positions hit
targets, accessible word rectangles, search highlights and selection.
Partly visible rectangles are clipped. Fully offscreen rectangles are
omitted while the page text remains readable through AT-SPI. Changing
the backing resolution preserves the logical zoom and normalized center.

Add `--pdf-zoom` with `--pdf-link-reader` to run the independent native
pixel and AT-SPI workflow. Use `--pdf-detail` with `--pdf-link-reader` to
exercise delayed, successful and rejected refinement results.

### PDF page thumbnails

Open **Pages** after the document index reports a page count. The dialog
shows up to three pages at a time. Tab moves through the cards and
navigation controls. Enter on a card opens that page. Escape closes the
dialog and restores the previous focus. The selected card marks the
current page. The dialog requires a logical window of at least 480 × 248.

The existing worker reads thumbnails from the pinned document with the
same crop choice as the page image. Page requests take precedence over
queued previews. An unavailable preview keeps its page card operable.
The UI owns up to three preview frames for the current group. Reopening
the same group reuses them. A new document, preview group or raster size
retires the old frames after the prior software render returns. Only
matching completions join the visible cache. Dialog colors and focus
indicators are drawn from the current theme around the page pixels.

The logical thumbnail size is 104 × 80. The fixture combines output and
page backing scales and caps the preview density at 4×. Use
`run_native.py --case pdf --pdf-thumbnails` with the normal renderer,
font and native test arguments to exercise a seven-page document.

### PDF document properties

With the optional Poppler-GLib helper enabled, choose **Pages → Properties**
to read the title, author, subject, page count and PDF version. Tab reaches
the scrollbars when the text overflows. Escape closes the properties dialog
and restores focus to **Properties**. AT-SPI can read the complete admitted
text while any part is scrolled out of sight.

The worker invokes the same helper with `--properties` on the pinned file
after the first page publication and index job for that document. It admits
a versioned reply with at most 512 UTF-8 bytes per field. Control characters
used as separators become spaces. Unset and unavailable fields have
distinct labels. The view checks document generation before adopting the
reply. A failed read keeps the page and available previews usable.

Add `--pdf-properties` with `--pdf-link-reader` to run the native keyboard,
scrolling and AT-SPI workflow.

### PDF viewport PNG export

With a destination supplied by the host, **Pages → Export PNG** captures
the current document page, zoom and pan. The worker renders the captured
viewport into an independent white canvas. It preserves the viewport
aspect ratio and centers it in the requested output. The current viewer
stays usable during export, and the dialog reports the result through
visible text and AT-SPI.

`content_pdf_export_render` accepts logical width and height and a scale.
It returns owned RGBA8 pixels at the ceiling of each scaled dimension.
The fixture requests 641 × 361 at 1.25× for an 802 × 452 PNG. The export
has a 4 MiB pixel limit and uses the pinned Poppler document. The PNG
writer completes a temporary file beside the selected destination before
publishing it. A name already in use keeps its original bytes and is
reported as a conflict.

Use `--case pdf --pdf-export` for the native workflow. It saves
`exported-page.png` under the private work directory.
