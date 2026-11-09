# Application-owned document presentation

Development SDK0.1.30 adds `nx_text_document_set_presentation` after either
existing document registration function. The application still owns its model,
layout, drawing, navigation, IME and clipboard. Call after handling an admitted
edit and after producing the geometry of that same resulting document. Supply
the exact current document identity/revision; stale tokens are rejected.
Existing public structures and registration signatures are unchanged.

The SDK copies boxes and attributes into this frame's semantic storage. No
layout/font/model pointer or callback is retained. Native Linux publication
copies the data again into its independently owned snapshot, before application
allocator callbacks. Native queries never call application code. The existing
32-MiB native snapshot bound includes these copies and alignment; publication
failure retains the previous native snapshot.

`nx_text_document_box` uses UTF-8 byte intervals on scalar boundaries and logical
window coordinates. A nonempty interval may represent a whole cluster/ligature;
all its native scalar offsets receive the same bounds. A zero-length interval
is an explicitly supplied caret rectangle. Keep boxes sorted by `(start,end)`;
nonempty intervals cannot overlap, and each boundary has at most one caret.
Caret records may lie inside a shared cluster/ligature interval. Character
queries skip those zero-length records and retain the shared character bounds;
empty-range queries return the explicitly supplied caret rectangle.
Visual positions may run in another order, including bidi text. Point queries
resolve overlapping rectangles in logical record order, returning that record's
first scalar. Geometry must come from the actual layout; the API does not infer
advances, wrapping, cell width or clipping from the document bytes.

Sparse geometry is explicit: an uncovered character or a range crossing a gap
returns NotSupported. Empty-range geometry requires an explicit caret. This
permits bounded viewport publication for large models without fabricating
offscreen positions. It does not establish full-document geometric coverage,
scroll-to-substring behavior or a measured large-layout performance budget.

Defaults apply to the complete document. Sorted nonempty, nonoverlapping
attribute runs override matching names; gaps inherit defaults. Names are unique
within each set, nonempty UTF-8 up to128 bytes; values are UTF-8 up to4096 bytes
and can be empty. Neither contains NUL. Use the native consumer's text-attribute
vocabulary, such as `family-name`, `weight` or `size`; the core applies no CSS or
font resolution. Empty defaults/runs explicitly publish an empty attribute set.

The limits are65536 box records,4096 runs,32 attributes per set and8 MiB for one
complete owned presentation. They are independent of the8-MiB document-value
bound. Validation and staging precede the context allocator. Failed validation
or allocation leaves this frame's document presentation unattached and permits
a retry. A successful attachment is allowed once per document per frame. Omit
it in a later frame to withdraw presentation while retaining ordinary text.

Linux AT-SPI implements GetCharacterExtents, GetRangeExtents, GetOffsetAtPoint,
GetAttributes, GetAttributeRun, GetAttributeValue and GetDefaultAttributes for
these snapshots. Native scalar indices are converted independently of byte
offsets. GetAttributes omits defaults; GetAttributeRun merges them only when
requested; GetAttributeValue includes inherited values. Unknown names return
an empty string. Native rectangle rounding and WINDOW/PARENT translation reuse
the existing Component path. SCREEN is NotSupported when the compositor does
not supply that origin. Point queries respect native ancestor scroll clipping;
a point without a published character returns-1.

Changed style data emits TextAttributesChanged; changed box data emits
VisibleDataChanged. Identical repeated snapshots emit neither event. Windows
and macOS currently retain the portable presentation in the context but do not
expose it through their native Text/TextRange geometry/attribute adapters.
This entry supplies no new ordinary editor layout or input behavior.

The independent libatspi fixture exercises the actual private Wayland transport.
Its rectangles are declared synthetic geometry, not the original Editor's
rendered text. Original Editor layout integration, installed consumer/complete
CI acceptance, real screenreaders/IMEs and physical/human/design/performance
acceptance require their own evidence; all103 Aetheri requirements remain active.

Protocol references: [AT-SPI Text wire contract](https://raw.githubusercontent.com/GNOME/at-spi2-core/main/xml/Text.xml)
and [attribute-run semantics](https://gnome.pages.gitlab.gnome.org/at-spi2-core/libatspi/method.Text.get_attribute_run.html).
