# Accessibility semantics and native transport

NexaUI records a backend-independent semantic tree in each UI context. Nodes carry a stable ID, parent, logical bounds, role, accessible name, description, value, state and supported actions. Windows has an initial UI Automation (UIA) transport. Native macOS AX and Linux AT-SPI transports remain pending; the semantic API and keyboard behavior are shared by all platforms.

Call `nx_semantics_add` during frame construction in parent-first order. A parent ID of zero creates a root. IDs must be unique in that frame; use the same stable ID for a semantic node and its interactive region. Interactive roles require a nonempty accessible name. Strings are copied, validated as UTF-8 and rejected if they contain embedded NUL bytes. `nx_widget_register` binds the semantic node and optional interaction region together; built-in widgets use this path.

After `nx_end_frame`, `nx_semantics_count`, `nx_semantics_get` and `nx_semantics_find` expose the completed tree. String views are borrowed until the next `nx_begin_frame` or context destruction. Storage and lookup tables are reused across frames. `NX_SEM_FOCUSED` comes from `nx_focus_id`. Disabled or inactive regions, including interactive controls outside the active modal overlay, are also reported as `NX_SEM_DISABLED`. Widget-supplied state includes checked, selected, invalid, required and protected flags.

## Owner-thread actions

`nx_semantics_request_action(context, id, action)` accepts press, focus and selection requests against a completed frame. It validates both semantics and the interaction region. Missing, disabled, inactive and modal-blocked controls are rejected. Unsupported actions are rejected. Press/focus and legacy ungrouped select requests use ordinary interaction in the next frame. Explicit selection groups instead update an application-owned selected ID through the selection APIs; selecting an item does not invoke its command or synthesize a click.

The context queue has 64 entries. A duplicate ID/action or a full queue returns `NX_ERROR_LIMIT` without accepting another request. Context calls, including this function, belong to the context owner thread. There is no arbitrary worker-thread access to a context, and accepting a request does not mean the application has completed its action. Increment/decrement, editing, expansion and text-range actions remain separate work.

## Explicit single-selection groups

`nx_selection_group(ui, &semantic_desc, required, &selected_id)` registers a named GROUP, LIST, TREE or TAB_LIST container. It applies queued selection requests to the caller-owned widget ID, then the application builds SELECT-capable descendants using that ID. Ordinary semantic registration cannot set the derived `NX_SEM_SELECTION_SINGLE` or `NX_SEM_SELECTION_REQUIRED` flags. The model pointer is never retained. If layout depends on selection, call `nx_selection_apply(ui, group_id, required, &selected_id)` before calculating layout; requests are consumed once, and later group registration preserves that result.

```c
uint64_t selected_id = app->selected_file_id; /* Widget ID, or zero. */
nx_semantic_desc group = {0};
group.id = 100;
group.role = NX_ROLE_LIST;
group.name = (nx_utf8_span){"Files", 5};
group.bounds = file_list_bounds;
/* During a frame, before building this group's items: */
nx_result result = nx_selection_group(ui, &group, 0, &selected_id);
if (result != NX_OK) return result;
app->selected_file_id = selected_id; /* Persist across frames. */
/* Build items parented under this group, with SELECT capability and
   NX_SEM_SELECTED iff their ID equals selected_id. Check every result. */
```

The nearest explicit ancestor determines membership, including items inside intermediate panels. Nested groups remain independent. Required groups have exactly one selected member; optional groups allow zero or one. TAB_LIST is required single-selection. End-frame rejects invalid cardinality, so native publication cannot expose two selected items in a declared single-selection group. Radio membership uses CHECKED; other members use SELECTED. Built-in tab/radio visual reconciliation follows the explicit group across intermediate parents.

SELECT replaces the ID. The derived ADD_SELECTION capability only fills an empty selection or repeats the same ID. Optional members also derive DESELECT, which clears only a matching ID. Required selections cannot be cleared. Applications with radio domain option numbers must map them to widget IDs (or use the widget ID as the option). The application remains responsible for projecting a changed/filterable domain model into the displayed member states. Failed group registration preserves both the model and queued requests; successful early apply is an intentional application-model update before subsequent layout work.

## Initial Windows UIA bridge

Publish each completed native frame on the window/context owner thread:

```c
/* One normal application iteration; check every result in application code. */
nx_window_poll(window, 0);
nx_window_route_events(window, ui, &routed);
nx_begin_frame(ui, width, height, elapsed_seconds);
/* Build widgets and application UI. */
nx_end_frame(ui);
nx_window_publish_semantics(window, ui);
/* Render and present the completed drawing commands. */
```

The Windows NexaCloud runner uses this publication path. Publishing owns a UTF-16 copy of the finished semantic tree; later frame construction or context destruction cannot invalidate native query data. Two reusable snapshot allocations are bounded to 32 MiB each, with at most 65,536 semantic nodes per snapshot. Allocation, invalid input or limit failures preserve the previous publication. Native COM provider objects and OS marshalling resources have their own lifetimes outside that snapshot budget.

UIA callbacks read the current snapshot under an SRW lock. They never dereference a context or window object and never call application widget code. COM providers use a free-threaded marshaler and retain a small native bridge independently of the HWND. Snapshot allocations are reclaimed on the owner thread when the bridge is detached. The caller's window allocator must remain valid through window destruction, as with other window-owned storage.

The fragment root is the native HWND; semantic roots appear below it. Parent/child/sibling navigation, fragment hit testing, stable runtime IDs, logical-to-physical client bounds, enabled/focused/protected state, names and descriptions are present. A semantic window node maps to a pane below the native window. A bridge epoch and per-node incarnation distinguish removed/recreated or retyped controls, replaced contexts, detach/reattach and reused HWND values. Continuity refers to consecutive published snapshots; applications must still assign IDs consistently.

The bridge advertises Invoke for press-capable controls, Toggle for checkboxes/switches, and Selection/SelectionItem for explicit single-selection groups and their members. Selection exposes its current zero/one-element array, required flag, membership, and Select/AddToSelection/RemoveFromSelection. Adding a different member to an occupied single selection and removing the required selected member are rejected. Focus on a group delegates to an eligible selected member, or its first eligible member. Multi-selection, editable Value/Text/TextRange, RangeValue, ExpandCollapse, Grid/Table and scrolling patterns remain pending. Protected nodes publish an empty value even if a custom widget supplies plaintext. The password widget itself also keeps its semantic value empty.

Native requests enter a separate bounded 64-entry queue and are forwarded by `nx_window_route_events` on the owner thread. Repeated focus requests coalesce to the latest requested target. Duplicate press requests and full native queues return a busy error. A full context queue defers already accepted native work until a later routing/frame iteration. One native selection transaction per group may remain pending through a genuinely newer completed publication. An identical pending operation is idempotent; a conflicting one returns busy. Republishing the same completed frame does not acknowledge it. This also keeps AddToSelection admission consistent while the owner has not applied an earlier operation. A stale or newly disabled target is discarded rather than redirected. Native HWND enablement is copied on the owner thread, including `WM_ENABLE`: a disabled host reports its root and descendants disabled, rejects new requests, and cancels native work that has not yet been forwarded to the context. Re-enabling it does not replay those canceled requests. Route through the context that was most recently published: a different context returns `NX_ERROR_STATE` before consuming window events. If the live context has advanced ahead of its published snapshot, native requests wait for publication and are then revalidated. A queued selection includes its original group identity, so moving an item between groups does not redirect an earlier request. Publishing a different context invalidates old elements and pending requests; a process-wide identity prevents pointer-address reuse from retargeting them.

`nx_window_publish_semantics(window, NULL)` detaches the bridge. Retained element handles become unavailable; teardown may also surface an OS COM disconnection error. Reattaching creates a new bridge epoch. The application must keep the NexaUI module loaded until external provider references are released; safe arbitrary DLL unloading while clients retain elements is not accepted yet. The bridge balances its own COM initialization without uninitializing a caller-owned STA/MTA apartment. Windows' message pump must continue while clients and the application are still using that apartment, including after a particular window is destroyed.

Event transport reports structural invalidation, actual published focus changes and committed selection changes. Selection flags, current arrays and events follow the completed model projection. Selected/removed and IsSelected property-change notifications use copied provider identities outside the snapshot lock; group selection changes invalidate its selection. No-op selections do not fabricate model changes. COM QueryInterface keeps a static interface set; GetPatternProvider and each operation separately validate current UIA capabilities. It does not synthesize Invoke completion when a request is merely queued. Name/value/toggle property-change events, text changes, live regions and richer announcements remain pending. Bounds use window DPI and client origin; full ancestor clipping and offscreen behavior beyond client intersection still need acceptance work.

## Validation and remaining acceptance

`nexaui_semantics` checks the portable tree, storage, focus, modal rejection, action delivery and backpressure. `nexaui_uia`, `nexaui_uia_mta` and `nexaui_uia_owned_com` launch an independent native UIA client process that links no NexaUI code. It exercises native queries and patterns, Unicode and long titles, protected values, moved physical bounds, live snapshot publication during queries/actions, full queues, allocation rollback, removed/recreated controls, context replacement/destruction and detach/reattach/window teardown. The host also includes the SDK's C UIA client header and checks the private identifier values, detecting static-link symbol collisions with C consumers.

Run the Windows tests from a desktop session:

```powershell
ctest --test-dir build -C Debug -R '^nexaui_(semantics|uia.*|nexacloud_uia.*)$' --output-on-failure
```

The [native validation report](../quality/windows-uia-report.md) and [selection follow-up](../quality/windows-uia-selection-report.md) record the executed environments, results and remaining gaps. The UIA fixture briefly shows and focuses its own test window. `nexaui_nexacloud_uia` runs that independent client against the real native D3D12 NexaCloud loop, including page navigation, file selection/preview, clearing, and view removal/reopening. Its compact companion exercises the responsive details dialog and waits for its modal interaction scope to close. Retired pattern status accessors can normalize unavailable data to false/empty in the checked Windows runtime; the tests also require element unavailability and rejected mutations, and verify that the new model is untouched. An automated UIA client is not a screenreader acceptance test. Narrator tasks, multi-selection, physical DPI/large-text checks, complete UIA control-pattern/event coverage, macOS AX, Linux AT-SPI and cross-platform screenreader scenarios remain open (W07/X05/X08, M07/X06, L11/X07).

The [headless accessibility example](../../examples/12_accessibility/README.md) demonstrates tree inspection and keyboard traversal. [Custom widgets](../../examples/09_custom_widget/README.md) share the same semantics/interaction registration path.

Native design references: Microsoft [server-side providers](https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-serversideprovider), [client threading](https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-threading), [provider options](https://learn.microsoft.com/en-us/windows/win32/api/uiautomationcore/ne-uiautomationcore-provideroptions), [runtime IDs](https://learn.microsoft.com/en-us/windows/win32/api/uiautomationcore/nf-uiautomationcore-irawelementproviderfragment-getruntimeid), [HWND teardown](https://learn.microsoft.com/en-us/windows/win32/api/uiautomationcoreapi/nf-uiautomationcoreapi-uiareturnrawelementprovider) and [provider disconnection](https://learn.microsoft.com/en-us/windows/win32/api/uiautomationcoreapi/nf-uiautomationcoreapi-uiadisconnectprovider).
