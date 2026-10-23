# Multi-selection contract and implementation plan

Status: portable, Windows and AppKit implementation published through
`12ce9c5b549201773bb19f0ff6888437be6982fe`; the AT-SPI addition is locally under validation.
This document defines the requested complete contract; a model-only test is not
acceptance of native multi-selection. Existing single-selection APIs keep their
signatures, public layouts and behavior.
The published portable/Windows/AppKit scope passed independent review and exact
full CI. The new Linux adapter has separate isolated native and component evidence
in the [AT-SPI report](../quality/linux-multiselection-report.md); its independent
review and exact full CI remain gates. Physical/assistive acceptance stays open.

## Requirements and observable acceptance

| ID | Requirement | Required evidence |
| --- | --- | --- |
| M1 | A bounded logical set supports exclusive Select, additive Add and Remove, including an empty optional selection. | Portable C17 model tests, actual selected IDs after every operation. |
| M2 | Membership, selected IDs, active row and keyboard focus are distinct. | Reorder/removal tests; active changes preserve selection; selection changes preserve active/focus. |
| M3 | Hidden rows remain logical members; removing a row prunes its selection and active ID. | Virtual-range transitions with selected offscreen IDs; deleted/reused IDs cannot receive an old request. |
| M4 | Every update and queued batch is atomic on invalid input, limits or allocation failure. | OOM at each allocation, invalid second operation after a valid first, exact old-state comparison, sensitive negative controls. |
| M5 | Native owners advertise CanSelectMultiple and GetSelection returns the complete published selected set. | Independent external COM client checks both strict properties and the exact returned ID set, including an offscreen selected member. |
| M6 | Select/Add/Remove reach the owner and are revalidated against model identity/revision, owner, membership, disabled/modal scope and native lifetime. | Real model-reflection checks plus context/disable/detach/recreate/parent/policy stale controls. |
| M7 | Native selection/value events describe committed snapshots and survive reentrancy. | External copied event arguments and final ID-set checks; no event for admission alone. |
| M8 | List and table composition work with bounded, independent groups. | Two list rows and two table rows selected externally; remove one; untouched peer group; nested owner checks. |
| M9 | Existing single-selection, EditValue and ABI contracts remain valid. | Relevant regression, public struct/export comparison, installed C17 consumers; final independent review and exact-head full CI. |

The user manual-test pause, no app-pin change and no other-chat messages remain
in force. On2026-10-22 the user authorized main publication after the applicable
checks are green and no review concerns remain; the prior reviewed SDK5ede570
has been published, while this incomplete feature remains local. The user also
reported the Mac available again;
new required platform tests may use it. Windows native tests must use
the previously proved private desktop/job isolation. Physical/IME/MixedDPI/AX
acceptance remains separate.

## Architecture decision

Two structures were considered:

* Extending caller-owned `uint64_t *selected_id` to a mutable array retains simple
  ownership, but cannot identify model replacement/address reuse, control growth,
  or reserve space for an atomic batch without a larger caller transaction API.
* A separate opaque `nx_selection_model` owns a bounded logical membership catalog,
  canonical selected IDs and staging space. It permits validation before commit,
  independent model lifetime/revision and allocation-free bounded action batches.

The opaque model is chosen. Applications still own domain/file data and labels.
The model holds only stable widget IDs and disabled membership flags; it is not
a file-list store. Its catalog order determines deterministic selected-ID order.
A sorted ID-to-ordinal index provides binary lookup without adversarial hash
chains; membership replacement builds an independent allocation before commit.
Action batches copy/validate in prepared staging space, then commit once.

## Portable model API

`nx_selection_model_create(max_items, allocator, &model)` accepts an explicit
positive limit up to65,536. `set_items` copies the complete logical catalog,
preserves the intersection of selection and clears an active ID that disappears
or becomes disabled. Reordering preserves selection by ID. Virtual scrolling
changes realized widgets, not this catalog. A removed/reintroduced key is a new
membership revision, even if the numeric widget ID is reused.

`set_selected` is the application's authoritative whole-set replacement. It
requires unique catalog IDs; disabled IDs may remain selected by application
policy, as when an already selected row becomes disabled. This does not move
the active ID. Both application setters advance the invalidation revision even
when replacing with equal data. Input may borrow the model's current view.

`apply(changes, count, required, &changed)` accepts at most64 Select/Add/Remove
operations. Unknown/deleted/disabled targets fail STATE; malformed action/zero ID
fails ARGUMENT. Select replaces the set, Add is idempotent, Remove is idempotent.
The final required selection must be nonempty; a remove-then-add batch may replace
its last item atomically. Failure changes neither selection nor active ID.
No allocation occurs during action application. A successful changed batch
advances the revision once; an idempotent batch does not.

`set_active` validates an enabled catalog ID or zero and never changes selection.
It does not request host/keyboard focus. A borrowed `get` view exposes catalog,
selected IDs and active ID until the next successful model mutation. Owner-thread
calls only; callers keep the model and borrowed inputs alive through each call.
Allocator callbacks may not destroy the model. Reentrant model reads/mutations
during an allocating setter return STATE; they cannot observe partial storage.
Identity/revision counters saturate; they never wrap into a previous target.

## Context and Windows provider binding

Two explicit bindings now attach the model to existing semantic nodes:
`nx_selection_bind_owner(context, owner_id, model, required)` and
`nx_selection_bind_item(context, owner_id, item_id, offscreen)`. Owner binding
must precede selectable row descendants; existing table headers may precede it.
The existing list/table/group descriptor and table
headers remain their single semantic registration. Item binding uses an already
registered descendant, validates its nearest selection owner and logical catalog
ID, and derives selected/disabled state from that owner's model. This supports
existing table-row registration without inventing a second table or row ID.
Sorting COLUMN_HEADER actions remain separate from row membership. Existing
painted `nx_table_row` containers still require positive dimensions; logical
zero-area rows can use `nx_semantics_add` with the ROW role. No container
validation was relaxed for offscreen projection.

Offscreen members have copied logical semantics but no hit/focus region. Their
selection actions remain available when the group is enabled and in the active
modal scope. Focus still requires a realized focusable region. At end_frame,
every selected model ID must have exactly one bound member. All groups prepare
their final sets and validate complete projections before the first model commit;
a missing selected member rejects the frame without partly editing another group.
Adding that missing semantic member permits the same-frame end_frame retry.
Application catalog/selection replacement, including in an allocator callback,
invalidates old requests. Finalization reflects the current model, including
clearing selection capability on a bound row removed during the frame. It cannot
publish a truncated selected set.
Catalog and model ownership stay with the owner thread through frame completion.
Completed snapshots contain scalar target identity/revision, never model pointers.

Selection requests use a separate bounded scalar record with source frame,
owner/parent identity, model target, membership generation and an optional native
lifetime guard. They cannot use
the old region-only single-selection queue unchanged: that queue deliberately
discards targets without realized hit regions. At the following frame completion,
finalization validates the target and current policy after preceding allocation
and IME call-outs, then applies the valid bounded batch without allocating. Revoked,
reparented or replaced model work is discarded; resource backpressure must not
drop an accepted live request. A guard check itself may only inspect synchronized
native scalars and must not call application code. The bindings must not retain
the model beyond the documented frame lifetime or infer lifetime from its address.

Binding one model to multiple owners in the same frame will be rejected, so a
queued action cannot silently invalidate a second owner's published revision.
Nested owners remain distinct. Native selection getters must collect all matching
selected children rather than one selected index. Required-last-item checks use
the actual selected count. Existing single-selection behavior remains separate.

Completed context snapshots copy scalar model identity/revision and selected
membership; neither native COM queries nor queued requests retain an editor or
model address. The next owning group resolves/revalidates its live model before
applying a bounded batch. Context/ancestor/modal restrictions and the native
revocable lease must be checked after all allocator call-outs. Invalidated work
is dropped; resource backpressure retains live pending work for retry.

Semantic projection must include every selected logical member, including rows
outside the paint range. A selected hidden row must have a stable logical
provider with offscreen geometry, not disappear from GetSelection. A missing
projection must fail publication rather than truncate the set. Realized row
regions supply hit testing/focus; offscreen membership does not invent a hit
region. Table owner binding must compose with existing table/header registration
without duplicate table IDs. The context bindings above provide that projection
contract, beyond the original model API. The Windows
snapshot/COM adapter consumes it, including offscreen flags and the
copied scalar identity/revision and membership generation. A membership generation
survives reorder and selection changes; remove/reinsert gives that ID a new
generation even if no intermediate frame was published. Together with model
identity this distinguishes retained native handles. The independent external
COM client verifies stale handles after model/context/parent/membership changes,
including remove/reinsert without an intermediate published frame. Window
disable/enable keeps current element handles valid but revokes old pending work.

Native admission remains bounded and ordered per owner. Selected count, not one
selected-node slot, drives multi-selection getters and the required-last-item
check. Select/Add/Remove must be applied to the actual owner model before the
new snapshot acknowledges completion. Requests admitted against another model,
catalog revision or owner incarnation must not apply after remove/reinsert.
Events use copied IDs/incarnations and arguments outside snapshot locks.

Windows snapshots own a complete linked set of selected member indices. Native
GetSelection copies the complete scalar ID/incarnation set under the snapshot
lock using only the process heap, then creates providers/SAFEARRAY elements
after unlocking. It never calls the application's allocator or COM under that
lock. Per-owner native admission remains serialized until a different completed
frame is published. Unchanged live work survives context backpressure; stale
model revisions and revoked native leases do not. The model remains owner-only.

Selected/Added/Removed events follow the resulting count; a transition from two
members to one also raises Selected for the remaining member, without inventing
an IsSelected property change for it. Container invalidation compares the whole
set and member incarnations, independently of semantic ordering. Event arguments
are copied before UIA call-outs. The external client checks copied Automation IDs,
strict CanSelectMultiple/required properties, full selected sets through64members,
separate list/table owners and actual model acknowledgements. Realized and logical
offscreen members are distinguished while the private host HWND is visible only
on its isolated desktop. No global ElementFromPoint or physical input test is
included in that isolated validation.

## Primary-source decisions

Microsoft specifies a provider for each selected child in
[GetSelection](https://learn.microsoft.com/en-us/windows/win32/api/uiautomationcore/nf-uiautomationcore-iselectionprovider-getselection)
and the container/item responsibilities in the
[Selection guidelines](https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-implementingselection).
The [Select API](https://learn.microsoft.com/en-us/windows/win32/api/uiautomationcore/nf-uiautomationcore-iselectionitemprovider-select)
summary specifies exclusive selection, while its older remarks describe a no-op
for an already selected item. This contract explicitly chooses exclusive Select,
including when its target is already part of a larger set; that case requires a
direct native regression. [AddToSelection](https://learn.microsoft.com/en-us/windows/win32/api/uiautomationcore/nf-uiautomationcore-iselectionitemprovider-addtoselection)
documents selection events according to the resulting count. These sources were
read2026-10-22; they do not themselves prove this SDK's runtime behavior.

## Delivery sequence

1. Portable model and allocation/reentry foundation are locally verified.
2. Context owner/member binding, queued scalar revalidation, complete projection
   and atomic finalization are locally verified. New private guard hooks still
   require native evidence in addition to the portable tests.
3. Windows snapshot/query/action/event paths and external list/table/offscreen/
   lifecycle/OOM/bounds cases are locally verified: six new native runs plus14
   existing native regressions under ASan/Release, all on private desktops.
   Lifecycle changes are checked both before and after forwarding; allocator
   disable/detach call-outs revoke already forwarded work. Three negative DLL
   controls use identical final host/client programs. AppKit now has permanent
   independent external AX regressions and reviewed full-CI delivery. AT-SPI
   remains a separate candidate; no native Text/RangeValue or physical/assistive completion.
4. Run necessary single-selection/EditValue/consumer/ABI regressions, freeze the
   complete candidate for independent review, publish the reviewed scope and
   attribute full CI to the exact commit. Application adoption is separate.
