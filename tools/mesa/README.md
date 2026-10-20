# Private Mesa lifetime candidate

This is a **candidate dependency correction**, not a NexaUI SDK dependency or
system driver installation. The ordinary CI workflow keeps its existing gates.
The separate paired workflow builds a private unmodified/corrected Mesa25.2.8
and requires the old failure before accepting corrected controls. It also runs
the complete original sanitized native suite in both ICD orders, including the
existing no-skip Wayland gate. No suppression, retained DSO handle, preload or
NODELETE flag supplies a passing result.

The two-file patch frees CPU affinity masks in a late ELF finalizer. The
published capability copy aliases the allocation and is not freed twice.
The rtasm pool releases its real mapping and metadata when the last allocation
is returned. Existing live owners retain their pool; failed initial allocation
or mapping does not strand the pool. Failed unmapping retains ownership for a
later retry. Calls remain serialized by Mesa's existing mutex.

Source hashes before/after applying the patch are in `25.2.8-lifetime.json`.
Unknown or already modified source is rejected before applying anything.
The official [Mesa25.2.8 checksum](https://docs.mesa3d.org/relnotes/25.2.8.html)
pins the source archive. `build-inputs.json` pins SPIRV-Tools2025.1, the exact
SPIRV-Headers revision from that release's DEPS, Meson1.8.3 and the protocol1.41
XML package named/hashed by Mesa's own wrap file. Source archives
are hashed before extraction. Only the isolated runner's private directories
are built; no Mesa installation step changes the system driver.

SPIRV-Tools stays enabled in both driver builds. Its source-library build omits
its own executables/upstream test suite; those tests are not claimed as run.
NexaUI's original sanitizer, Khronos validation and synchronization checks stay
enabled. Normal Vulkan object destruction must quiesce workers before module
unload; the correction is not a replacement for application teardown.

`check_mesa_cleanup.py` compiles the actual pinned allocator/u_mm and CPU state
sources with ASan/UBSan/LSan. It tests live executable memory, retained users,
OOM/retry, unmap refusal/retry and concurrent allocation/free pairs. Its CPU
fixture verifies that aliasing state is freed exactly once after both the
dependent exit callback and default finalizer. The CI fixture requires actual
`dlclose` execution; musl's non-unload result is reported as unavailable and
does not count as a passing unload check.

Mesa's original copyright/license notices remain in the pinned sources.
The modified source files carry their original MIT-style notices. Mesa's other
source licenses, SPIRV-Tools' Apache-2.0 license and SPIRV-Headers' per-file
notices remain in their archives;
no blanket license claim is made for the complete external source trees.
The retained binary artifact includes those complete corresponding source
archives, notices and the exact patch. Meson is private build tooling, not part
of the NexaUI SDK. Its original wheel/license is retained in the source bundle.
The candidate's acceptance requires actual paired CI results; local component
success alone does not establish a complete driver repair.
