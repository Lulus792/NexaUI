# Mesa global-lifetime cleanup candidate

2026-10-20. This continues the
[independent unload-leak reproduction](vulkan-lifetime-control-report.md).
The correction is a **validated private dependency patch** for the reproduced
Mesa 25.2.8 CPU/rtasm unload leaks. It changes neither NexaUI's SDK nor the
installed system driver or NexaCloud's canonical pin. Paired CI passes both
ICD orders, both validation states and two complete native sanitizer suites.
The [machine receipt](mesa-cleanup-candidate-ci.json) identifies the exact
source, binaries, negative controls and accepted run.

## Source and ownership change

The official Mesa25.2.8 archive is pinned by its
[published release checksum](https://docs.mesa3d.org/relnotes/25.2.8.html).
The [two-file patch](../../tools/mesa/25.2.8-lifetime.patch) and its
[before/after source hashes](../../tools/mesa/25.2.8-lifetime.json) are retained.
Unknown or already modified source is rejected before patching.

- CPU topology: a late ELF finalizer frees the L3 affinity allocation once.
  The published capability structure aliases that allocation; both stored
  pointers are cleared, without freeing the same allocation twice. Its GNU
  destructor priority101 follows default-priority module finalizers.
- rtasm: Mesa's existing mutex still serializes allocator changes. After a
  successful final block release, the fully coalesced free heap releases its
  actual10MiB executable mapping and metadata. Other live code keeps its pool.
  Failed initial metadata/mapping allocation leaves no stranded idle pool;
  failed unmapping preserves ownership for a later retry. Invalid sizes cannot
  overflow the existing rounding/int conversion. `free(NULL)` is harmless.

The [GNU priority contract](https://gcc.gnu.org/onlinedocs/gcc/Common-Attributes.html)
and actual finalizer callbacks are checked separately. Normal Vulkan destruction
must still quiesce workers before unloading the driver. This is not a substitute
for application object teardown, and does not claim to repair every pre-existing
CPU-discovery error path or every Mesa platform.

The inspected upstream `main` rtasm file is still byte-identical to25.2.8 at the
time of this investigation. The fetched CPU file retains dynamic affinity-mask
state. This is a statement about the inspected files/hashes, not an exhaustive
claim about all upstream changes or releases.

## Actual native component evidence

Alpine3.24.2/musl, Clang22.1.3, ASan/UBSan/LSan, actual pinned `u_mm.c` and
`rtasm_execmem.c`, real Linux mutex/futex and real `mmap`/`munmap`:

- The original allocator executes the generated function, then fails the
  retained-allocation/mapping assertion as expected. The corrected source
  completes, with tracked allocations/mappings zero and the mapping absent
  according to `mincore`.
- A live second owner retains its bytes after the first is released. Eight
  threads complete64 allocation/free pairs each, checking their live bytes.
- Initial metadata failure at each of three allocation points, mapping refusal,
  allocation failure with a live owner, failed unmap/retry, zero/oversize requests
  and null release all complete their expected checks. Real executable bytes
  return42 on x86_64; this is not a heap-only allocator substitute.
- The CPU fixture includes the actual CPU source and types. It seeds the two
  aliasing state slots, rather than replacing CPU discovery. Both dependent
  shutdown/default-finalizer callbacks read the still-live data; the new
  finalizer then frees it exactly once. Process-exit events are1,3,2.
  Local musl `dlclose` does not execute the unload callbacks: exit77 is retained
  as **unavailable**, not counted as successful unload coverage. CI requires
  real callback execution after `dlclose`.
- Modified-source/reapplication and wrong-archive hashes are rejected. A tar
  parent-path escape is rejected without writing outside its destination.

One earlier JIT fixture put its entry at the mapping boundary. Clang's indirect
call instrumentation read the eight bytes before that entry and faulted. The
retained executable disassembly identifies that read. The corrected fixture
keeps a32-byte zero prefix inside its allocation; no sanitizer is disabled.
Both the failed fixture and successful final component results are preserved.

## Paired driver build and current CI evidence

The separate workflow builds pinned SPIRV-Tools2025.1, its exact DEPS-selected
headers, Meson1.8.3 and Mesa's required protocol1.41 XMLs in private directories.
Both Mesa builds keep SPIR-V tooling enabled. Only two Mesa source files change
between the unmodified and corrected build. Complete corresponding source
archives/notices and the patch accompany retained runtime binaries.

The Mesa DSOs use `debugoptimized` with `b_ndebug=true`, without `b_sanitize`.
NexaUI, the independent Vulkan control and the native source-component fixtures
are instrumented with ASan/UBSan; LSan remains enabled for the host processes.
This detects the reproduced intercepted allocations at unload and exercises the
patched components directly. It does not instrument every internal Mesa load or
store. Nor does this establish hardware-driver, architecture, distribution-wide
or performance acceptance. The independently built before/after DSOs are each
identified by their own hashes and build IDs; they are not the Ubuntu package
binary from the original attribution report.

Run39491252463/source5666a0a builds the SPIR-V library but stops before a driver
build because `glslangValidator` is missing. The prerequisite is then supplied;
the frozen setup failure is not counted as a runtime regression result.

Run39492566166/source40fdd2e successfully builds the unmodified driver and
reproduces all eight known controls: Asahi-first unloads and leaks in both modes,
reverse order retains Lavapipe and passes, with correct pixels and layer states.
The component stage then stops at a missing mandatory CPU-close summary line.
The expected baseline128-byte LSan termination bypassed buffered stdio output;
both preceding callback events and its leak are retained. The strict gate is
kept; sourcec364e3d explicitly flushes that summary before termination.
No corrected driver/native-suite acceptance is inferred from that stopped run.

[Run 39494137019](https://github.com/Lulus792/NexaUI/actions/runs/39494137019),
source `c364e3d`, completes the unchanged candidate and all acceptance gates:

| Evidence | Original source | Corrected source |
| --- | --- | --- |
| Asahi first, device/draw, validation on/off | Four leak failures with actual unload | Four passes with actual unload |
| Lavapipe first, same four controls | Four passes with natural loader retention | Four passes with natural loader retention |
| Native CPU `dlclose` | Both dependent callbacks execute; no free; exit 1 and leak | Both callbacks execute before one free; exit 0 |
| CPU process exit | Events 1,3 | Events 1,3,2; exit 0 |
| Actual rtasm source | Expected retained-pool assertion failure | Executable memory, live users, OOM/retry and concurrent pairs pass; zero live allocations/maps |
| Full original suite, Asahi first | Not claimed by this paired run | 117/117; zero failures/skips; all seven mandatory Wayland results executed |
| Full original suite, Lavapipe first | Not claimed by this paired run | 117/117; zero failures/skips; all seven mandatory Wayland results executed |

All draw controls compare every one of the 1,024 RGBA bytes. Layer states,
runtime/disk build IDs, driver hashes and lifetime snapshots match the expected
before/after binaries. The original DSO SHA-256 is `99bc20ca0348de7f5e953fb1ae0400ff482665b373cc5f9d0b2f363ce1da9652`;
the corrected DSO SHA-256 is `7c8680599df392fbf8ade7a7217b6f28c4d1d71c6376d5049ef2b7213958585e`.
The locally downloaded artifact's hash matches GitHub's published artifact
digest; source-bundle hashes, JUnit counts/duplicates/skips and all sixteen
before/after control records were checked after download.

The parallel ordinary run 39494137109 still uses the installed stock Mesa:
its latest sanitizer suite passes 103/117 and reports leaks in fourteen Vulkan
cases. The six regular static/shared platform jobs pass; optional Windows-ASan
and fuzz jobs are skipped. This newer failure is preserved separately from the
earlier stock run's 117/117 result; loader retention must not be mistaken for
repair.

## Regular CI integration

The regular sanitizer job now builds this same pinned source pair privately,
requires the original unload failures, runs the component and corrected controls,
then runs the original complete native suite in both orders. The original ASan
counterexample, explicit Khronos/synchronization validation, independent module
diagnostics, ordered controls and no-skip Wayland gates remain. Both DSOs and
their complete corresponding source/notices are retained with the results.
The standalone paired workflow remains available manually; its duplicate push
trigger is removed.

[Regular run39496339864](https://github.com/Lulus792/NexaUI/actions/runs/39496339864)
at source `1cea7b94c958a9f4937135f4974857a287a7922f` completes successfully:
all six regular static/shared Windows/macOS/Ubuntu jobs and the sanitizer job
pass. Optional Windows-ASan and fuzz campaigns are skipped. The sanitizer's
deliberate ASan counterexample is detected; both complete suites pass117/117,
with no failure/skip/duplicate JUnit records and both seven-case Wayland gates.
The existing six module controls and eight additional ordered controls pass.
All pure first-order controls demonstrate actual driver unload. The downloaded
artifact digest, source bundle, driver hashes/build IDs and records were verified;
both DSOs are byte-identical to the earlier paired run's corresponding DSOs.
The integrated records are included separately in the machine receipt.

This runner's unmodified device-only controls unload without a reported CPU
leak; its unmodified draws still reproduce112bytes in two rtasm allocations.
That differs from the earlier runner's device128/draw240 observations. The
builder already permits a device-only zero-leak result while requiring the
original draw leak and actual unload. The instrumented seeded-alias CPU fixture
still reproduces the original128-byte leak and verifies the corrected free.
No CPU-feature explanation for the runner difference is inferred. The first
local artifact reader assumed all original device controls must leak; its
failed assertion is retained, and the integrated reader checks these actual
per-case results instead. No runtime, CI gate or test is rerun or relaxed to
change the observation.

This is a private CI runtime correction. Unmodified system packages and end-user
driver installations are not repaired by committing the patch. No upstream
submission or packaging adoption is claimed. Releasing an executable pool on
its last user can change allocation/mapping frequency; no performance improvement
or broad performance acceptance is claimed. No NODELETE/preload workaround,
suppression, relaxed pixel assertion or global OS/cache/driver setting supplies
a passing result.

R01's system-I/O permission gap, WSLg FIFO, mixed-DPI, physical input/IME and
human assistive/full-design acceptance remain separate open requirements.
