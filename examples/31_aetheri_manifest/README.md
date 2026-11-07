# Original Aetheri manifest hierarchy

This installed Linux C17 consumer reuses example 30's hierarchy and native host,
and the **original** Aetheri `ae_fixture_verify_manifest` decoder/hash contract.
It shows the complete declared file-path list of one caller-selected canonical
manifest. It never opens those declared paths, resolves/runs an action, opens a
catalogue, installs a payload or establishes publisher/permission trust.

Select an immutable original snapshot, not Aetheri's active working checkout.
`source-inputs.json` pins 20 original build/core/contract inputs at
`1fb45c7c21f914a2893457132b609be50ef4280d`; configuration verifies their contents,
normalizing CRLF to LF only. Original build/dependency checks remain in force.
OpenSSL 3.5.9–3.5.x belongs to the original optional Aetheri consumer; neither it
nor Aetheri becomes a NexaUI SDK dependency.

```sh
cmake -S examples/31_aetheri_manifest -B build-manifest \
  -DCMAKE_PREFIX_PATH=/path/to/installed-nexaui \
  -DAETHERI_SOURCE_DIR=/path/to/immutable-aetheri
cmake --build build-manifest --target manifest_hierarchy_native manifest_source_test manifest_hierarchy_core
ctest --test-dir build-manifest --output-on-failure
build-manifest/manifest_hierarchy_native FONT.ttf MANIFEST LOWERCASE_EXPECTED_SHA256
```

Append `--source MANIFEST EXPECTED_SHA256` for additional caller-selected sources.
Previous/Next buttons then choose among that immutable ordered input set; the
ordinal and complete expected SHA-256 are exposed through native read-only Text.
The input set is copied and outlives every worker. Each accepted ticket is bound
to its selected immutable source before a worker is woken. It never consults a
later UI selection. A source change retires the old context/provider, creates a
new view epoch, clears artifact-local selection and explicitly loads the new
source. It preserves focus on an enabled chooser button. Pending old work retains
its channel reservation and its result is ignored when it arrives. Returning to
a previously visited source creates fresh native objects, without retargeting
held Text/Action references. F4 and F5 retain the current source.

For controlled multi-source tests use `--test-control READ_FD WRITE_FD` after the
sources. The legacy single-source two-FD test invocation remains supported.
`run_source_switch.py` and `source_switch_atspi.c` check actual keyboard/native
source selection, two real held workers, full exact identities, stale results,
retained references through A→B→A→B and close. `--automatic` executes the same
source workflow without a release gate. This is an artifact-input selector, not
a filesystem picker, inventory or authenticated catalogue discovery service.

The expected digest identifies the exact bytes selected by the caller. Its
provenance and authentication remain caller responsibilities. This view offers
read-only metadata inspection, with no admission or operation capability.
Right expands/loads, Left collapses, F5 refreshes, F4 retires/recreates the view
and Escape closes it. Existing native assistive actions use the same view.
Errors remain errors until explicit Retry; collapse does not pretend to cancel
a worker. No frame timer/polling thread is used: real workers wake the owner
through its pipe and `nx_window_poll_with_fd`.

Each worker receives only a copied view/generation/branch key, the immutable
manifest filename and expected digest. It opens a regular, non-symlink manifest
without blocking on a FIFO, reads at most the original 131,072-byte limit plus
one overflow byte, calls the original verifier and copies all admitted paths
before freeing the borrowed decoder input. It never receives UI objects.
Arbitrary filesystem I/O still has no bounded completion guarantee; Close
retires the GUI first, then joins workers before destroying the channel/fonts.

The original format allows 1–32 strictly sorted unique paths of 1–256 ASCII
bytes. Those complete paths fit the updated example response, including the
terminating NUL. Within **one exact artifact**, canonical path ordinal gives a
stable row ID. Switching artifacts requires retiring the context/view and using
a fresh epoch; positional IDs must not be reused for different artifacts in the
same view. Refresh with changed/unverified bytes produces Failed, never a
different list under retained IDs.

The shared example reuses the existing Settings emergency grapheme-wrap helper
and measures wrapped row heights. Semantic names retain all original bytes.
Bounds below 280×160 and
text outside 12–32 remain unsupported. Page Up/Down reads a focused row taller
than the viewport without moving focus or continually revealing its top.
Real-font checks retain that scroll; the independent native peer verifies both
edges through actual keyboard input. Broader fonts/human reading remain open.
This is not a complete component
browser, general multi-branch/paged hierarchy or normal Aetheri integration.

`run_source.py` generates tests through the original Python encoder and frozen
vector. `run_native.py` uses a private compositor and independent libatspi peer.
Its extra two host FD arguments enable an explicit **test-only release gate**:
the worker has already read/verified/projected the actual manifest before the
driver releases its result. The driver cannot choose success or fabricate rows.
Production invocation above has no release gate and completes automatically.
The native workflow observes wrong-artifact failure, Retry, all 32 full paths,
last-row selection/focus/reveal or top-to-bottom reading, collapsed pending work, newer success followed
by an ignored older failure, view replacement and Close with held work.

Human/physical assistive review, services, source discovery, payload file
verification and actual Aetheri adoption remain separate gates. Existing Aetheri
sources, launcher, default presentation and SDK pin are unchanged.

The shared `manifest_document.h/.c` adapter owns immutable bytes and exposes the
original verifier's complete bounded fixture views until explicit free. This
keeps component/version/target, publisher, file/action declarations and dependency
IDs alive without copying borrowed slices into dangling pointers. The existing
`amh_read` path projection now uses this same bounded regular-file loader, while
`amh_project` retains its original caller-byte contract and output behavior.
Expected hash provenance, publisher trust, payload/installation and authority
remain external. [Example34](../34_aetheri_software/README.md#exact-manifestdependency-join)
uses the document only after exact artifact/component/publisher agreement with
an original selected-install observation. The standalone example31 continues to
check its own original `1fb45c7` pin independently of example34's `46e2a21` pin.
