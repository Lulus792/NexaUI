# Independent Vulkan lifetime control

Source: `6fe63dfcc1cdd493228f549fe0547f3024662f0a`, 2026-10-20.
The diagnostic executable links Vulkan and `dl`, with no NexaUI library or
implementation source. It observes loaded modules without opening or retaining
module handles. This change does not repair a driver or alter production code.

## Fixed controls and their oracle

`tools/vulkan_lifetime_control.c` has two modes. `--device` creates an instance,
debug messenger, physical/logical device and graphics queue, then destroys all
owned objects. `--draw` additionally renders a vertex-buffer triangle into a
16×16 RGBA image, copies it to host memory after a fence and compares **all
1,024 bytes** with opaque red. It destroys the image, buffers, memory, pipeline,
shader modules, framebuffer, render pass, command pool, fence, device, debug
messenger and instance. Validation errors, failed API calls or differing pixels
fail the control. Device idle/fence waits precede destruction.

Snapshots identify modules after device creation, completed drawing, device
destruction and instance destruction. They record each module's mapped base,
load extent and GNU Build ID. The Python runner hashes the actual module file
and compares its disk Build ID with the runtime value; missing/mismatching
identity or completion markers fail the diagnostic. These are observations of
normal loader behavior, not driver retention or leak suppression.

The runner executes each mode once with explicit Khronos/synchronization
validation and once without it. It verifies the validation library's presence
or absence. ASan/UBSan halt on error; LSan remains enabled. The slow allocation
unwinder and `LD_DEBUG=files` are used on glibc; musl uses the fast unwinder to
avoid the already observed recursive-unwind deadlock. The full original CTest
suite and Wayland requirements remain separate gates. This diagnostic is not
registered as a replacement for an existing CTest case.

## Actual local results

Alpine3.24.2/musl, Clang22.1.3, Mesa Lavapipe26.1.6, Debug ASan/UBSan/LSan:
**4/4 controls exit0**, both draws compare all pixels, validation enabled/removed
as intended, no sanitizer report. All runtime/disk module IDs match.
Lavapipe Build ID: `a9df98d9c0bf2fa2ffba544cd5f87621c19bd210`.
In all four processes it remains in the observed module list **after** instance
destruction. This is a local observation, not proof of identical glibc behavior
or a universal no-leak claim.

The actual link command contains the control's object, `libvulkan.so`, `dl`
and the sanitizer/toolchain runtime; no NexaUI archive or shared library.
SPIR-V was generated for Vulkan1.0 by glslang16.2.0 and checked against that
selected compiler. The generator normalizes source/hash line endings. It does
not promise identical output from a different compiler version.

Two negative controls protect the diagnostic's meaning:

- A private source variant changes only the triangle's vertex count to zero.
  It builds successfully, fails the full-pixel assertion and exits1 after
  cleanup, with no validation or sanitizer error. The expected pixels cannot
  be supplied by the clear operation alone.
- A private empty executable script returns0 for every invocation but prints
  nothing. All four child processes really exit0, while the runner exits1 for
  missing completion/snapshots/driver identity. The order-comparison runner
  also rejects its eight empty successful child processes. The earlier
  `/bin/true` probe is retained separately: on Alpine its resolved executable
  is BusyBox, so it does not establish an exit0 child counterexample.

The [local receipt](vulkan-lifetime-control-local.json) retains exact commands,
source/executable hashes, module identities, results and hashes of raw evidence.
Raw local evidence is under `out/vulkan-lifetime-control/`. The initial failed
callback-prototype compilation and the earlier no-attribute pilot are retained
separately; neither is counted among the four final controls.

## Driver source boundary

The matching Mesa25.2.8 primary source for `rtasm_execmem.c` has static
`exec_heap`/`exec_mem` state. `init_heap` allocates its shared heap; individual
`rtasm_exec_free` calls return blocks to that heap, without destroying the
global pool. Its Mesa26.1.6 source is byte-identical. CPU detection stores the
allocated L3 affinity masks in static CPU capability state. These facts support
investigating the driver's DSO/global lifetime after the
[allocation-site attribution](vulkan-lsan-module-report.md). They do not yet
prove the complete cleanup cause or validate a dependency patch. Merely using
the newer local Mesa version is not an established repair.

Primary source URLs and hashes are retained in the receipt:
[Mesa25.2.8 executable-memory allocator](https://gitlab.freedesktop.org/mesa/mesa/-/blob/mesa-25.2.8/src/gallium/auxiliary/rtasm/rtasm_execmem.c),
[CPU detection](https://gitlab.freedesktop.org/mesa/mesa/-/blob/mesa-25.2.8/src/util/u_cpu_detect.c).

## CI and remaining acceptance

The preceding documentation-only source4f05d8d had a successful
[run39481622246](https://github.com/Lulus792/NexaUI/actions/runs/39481622246):
seven required jobs passed, including sanitizer117/117 and both native-clear
controls. The two optional jobs were skipped. No implementation repair occurred
between this green result and the earlier red runs. Those failed records remain
valid; one subsequent success does not close the intermittent finding.

The independent control's first [source-specific CI run39484227995](https://github.com/Lulus792/NexaUI/actions/runs/39484227995)
has sanitizer117/117, two native-clear and four independent controls passing.
Runtime/disk Lavapipe Build ID is
`b3fb0cf7870efc6646e21939393d3d2ad05cf1dc`, SHA256
`5a218118760fee2398cc783820a1cc052de162d8c937dafcd6045576dfd9576c`:
the actual CI library matches the retained official package library. This
closes the new run's identity gap; it does not retroactively hash an older
process's driver. Artifact12291934602 retains its snapshots and raw logs.

In **all four glibc controls**, Lavapipe remains loaded after instance
destruction. Loader output explicitly activates `NODELETE` for Lavapipe. In
the earlier red process8158, it instead activates that state for Asahi and
destroys Lavapipe's link map before the leak report. This is observed loader
behavior, without application `NODELETE` flags or retained handles. A simple
musl-versus-glibc explanation is therefore insufficient.

The package Lavapipe ELF exports five `STB_GNU_UNIQUE` symbols from SPIR-V
Tools. The [glibc2.39 symbol-lookup source](https://raw.githubusercontent.com/bminor/glibc/glibc-2.39/elf/dl-lookup.c)
marks a newly registered unique-symbol provider as non-unloadable. Khronos
documents that directory discovery order varies and allows process-local
[explicit manifest lists](https://github.com/KhronosGroup/Vulkan-Loader/blob/v1.3.275/docs/LoaderDriverInterface.md#driver-discovery-on-linux).
Together these provide a concrete **hypothesis** for the changing driver
lifetime, rather than a proved complete cleanup cause.

Source47813ab adds `tools/trace_vulkan_icd_order.py`: two fixed manifest lists,
Asahi→Lavapipe and Lavapipe→Asahi, each executing the four independent controls.
It changes only those diagnostic children's environment, records both manifests
and their hashes and propagates sanitizer failures. Original CTest and normal
native controls retain their original driver discovery. The comparison does
not force a retained driver as an acceptance workaround; it tests both orders.
No identical rerun campaign is used to obtain a green result.

## Executed order counterexample

[Run39485330188](https://github.com/Lulus792/NexaUI/actions/runs/39485330188),
source47813ab, executes both orders on the **same runner with the same executable
and driver bytes**, in fresh processes. Each row represents two controls:
explicit validation enabled and removed. Both produce the same outcome.

| Manifest order | Mode | Completed/pixels | Lavapipe after instance destruction | LSan / exit |
| --- | --- | --- | --- | --- |
| Asahi → Lavapipe | Device | Completed | Unloaded | 128 bytes /1 |
| Asahi → Lavapipe | Draw | Completed; all1,024 bytes equal | Unloaded | 128+56+56=240 bytes /1 |
| Lavapipe → Asahi | Device | Completed | Loaded | No report /0 |
| Lavapipe → Asahi | Draw | Completed; all1,024 bytes equal | Loaded | No report /0 |

All eight controls verify the validation-layer state and actual runtime/disk
driver identity. The first order activates loader `NODELETE` for Asahi and
destroys Lavapipe's link map. The reverse order activates it for Lavapipe.
The pure Vulkan draw reproduces both56-byte allocations without NexaUI linkage.
Same-process snapshots identify the direct allocation at offset0x150de3
(`get_cpu_topology`) and indirect allocations at0x4504b1/0x45048d (`u_mmInit`),
using the matching official ELF/debug symbol ranges. Different PIDs' ASLR bases
are not combined.

This establishes a concrete trigger: **driver discovery order changes whether
Lavapipe unloads and its global allocations become unreachable**. The source
and GNU-unique symbol mechanism explain a plausible route to the retention;
the exact symbol binding responsible has not been traced. No application
retention flag, preload, suppression or global driver setting supplies the
successful half of the comparison.

The normal sanitized CTest suite still passes117/117 and the six ordinary
attribution controls pass, but the **sanitizer job is failed** because all four
Asahi-first cases report leaks. These failures propagate normally and remain
retained. All six ordinary linkage jobs pass: Windows115/115, macOS96/96 and
Ubuntu117/117, each static/shared. The two optional jobs are skipped. A green
reversed order is not a fix. The
[CI receipt](vulkan-lifetime-control-ci.json) includes artifact12292509199,
all case records, runtime identities, source-specific job status and the eight
symbolized allocation records.

The next corrective boundary is dependency cleanup under real driver unload:
free the CPU-affinity/global executable-memory pool with a verified lifecycle,
then require both discovery orders, validation states and the original full
suite to pass. No such patch has been implemented or validated here. The
reproducer now makes that work reviewable without changing NexaUI's SDK or
NexaCloud's canonical pin.

This diagnostic does not close R01 startup, WSLg FIFO, physical mixed-DPI/input,
human assistive acceptance or NexaCloud's complete reference-design acceptance.
