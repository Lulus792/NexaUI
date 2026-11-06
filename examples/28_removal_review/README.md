# Owner-supplied removal review

This external C17 example composes installed NexaUI controls, text, semantics and
scrolling. Its bounded, copied plan distinguishes six ownership outcomes: removed
objects, shared dependencies, deliberate retention, independent documents,
backups/remote copies, and unknown/unmanaged state. Every entry preserves its
stable semantic ID, opaque object identity, reason and logical size. Logical size
is not a reclaimed-space or secure-erasure estimate.

The application owner supplies the plan, installation, scope and revision. These
are display/correlation references, not authorization. An incomplete inventory,
unknown object, invalidated plan or empty removal set disables the request. The
user must explicitly mark the scope as reviewed. A request returns the exact
copied target once; it neither accepts nor executes a backend operation. The
owner must revalidate the complete plan and current permissions before accepting
it. Close only closes the view. Retain the same owner across view replacements:
an issued request remains unknown and cannot be emitted again by reopening.

This is an example presentation seam, not an Aetheri management protocol. The
immutable Aetheri `1fb45c7c21f914a2893457132b609be50ef4280d` source documents this
ownership split in `brains/aetheri/knowledge/app-lifecycle.md` and
`launch-access-and-removal.md`, but contains no full removal-plan API. Fixtures
therefore supply declared plans; no real inventory, authorization, deletion,
service reconciliation or durable recovery is claimed. No Aetheri source or
consumer pin is changed. Real host integration, native assistive clients and
human/physical acceptance remain separate gates. The composition accepts up to
100,000 declared inventory objects and rejects oversized inputs in full. The
example-only `removal_plan.entries` descriptor is now a borrowed count-element
input; its metadata and all entries are copied before creation succeeds. The
installed SDK API/ABI is unchanged. Caller order is preserved within each of the
six dispositions, with sorted duplicate-identity checks at snapshot creation.

For more than 32 objects, the existing SDK Fenwick index measures visible rows
on demand. A bounded 256-row cache lasts only through one frame call, including
its geometry-settling passes. The view publishes the final geometry; a returned
request is committed to the retained owner only after the complete call succeeds.
Small previews retain their full semantic tree. Large previews always publish
the global identities and six group counts, plus visible/overscan object nodes.
Native scrollbar increment/decrement, arrows, Page Up/Down and Home/End expose
the full view without an object-sized semantic tree. Hidden unknown entries still
disable acknowledgement and requests. Logical extent uses estimated heights until
visited; exact remote offscreen lookup, filtering and real owner transport remain
open. The record copy occupies `count * sizeof(removal_entry)` bytes, plus indexes,
view state, fonts and framework resources; total process footprint needs its own
measurement.

Build against an installed 0.1.23 SDK:

```sh
cmake -S examples/28_removal_review -B out/removal-review \
  -DCMAKE_PREFIX_PATH=/absolute/sdk/prefix -DNEXAUI_REMOVAL_TEST_FONT=/absolute/font.ttf
cmake --build out/removal-review
ctest --test-dir out/removal-review --output-on-failure
```

The owner and fonts outlive every view. Calls stay on the owner thread. This
example does not define selectable retention policy, authenticated transport,
application persistence, completion receipts or installation/removal execution.

For a Linux SDK with native software hosting, `removal_review_native FONT.ttf`
opens a standalone fixture window. Its ordinary event loop waits for events and
cleans up semantics, renderer, view, window, owner and fonts on Close/Escape.
The private control pipe is enabled only by the test driver. Native resize below
420×320, output scale/loss, real locales and other platforms are not accepted here.

When libatspi development files are available, CMake also builds an independent
`removal_review_atspi_client` without linking NexaUI. Run it through the driver:

```sh
python3 examples/28_removal_review/run_native.py \
  --server /absolute/aetheri-test-compositor \
  --host /absolute/removal_review_native --client /absolute/removal_review_atspi_client \
  --registry /absolute/at-spi2-registryd --runner /absolute/NexaUI/tests/run_atspi_native.py \
  --font /absolute/font.ttf --work-dir /absolute/fresh-evidence-directory --keyboard --capture
```

Omit `--keyboard --capture` for native assistive actions; use `--incomplete` for
the disabled-plan/Close path. `--expect-unavailable` requires a separately built
host whose SDK has AT-SPI disabled, and checks failure at review discovery.
Add `--large` for the complete 100,000-object fixture. The keyboard case traverses
to the scrollbar, reaches the last object with End, returns to acknowledgement,
requests once and closes a replaced view. The assistive case uses a native
scrollbar increment and checks a newly visible object. `--large --incomplete`
places the unknown object at the far end while the initial view remains blocked.
All fixtures and processes are private. Captures and JSON preserve exact artifact
hashes and the limited acceptance scope; no human screenreader result is inferred.

With an explicit test font, CTest executes the small-view regression, the 100,000-
object workload and (on Linux) constructor allocation-failure checks. The large
workload includes viewport/font changes, 64 wheel-driven frames, offscreen unknown
state, full-snapshot duplicate detection and a request during geometry settling.
Printed times cover snapshot creation and frame construction/validation, excluding
rasterization and presentation. They are local measurements without an agreed
adoption budget. No terminal/Files stress or whole-catalogue acceptance is implied.
