# Vulkan LeakSanitizer module attribution

Follow-up: the [independent Vulkan device/draw control](vulkan-lifetime-control-report.md)
adds full-pixel validation and runtime/disk driver identities without NexaUI
linkage. Its executed two-order comparison reproduces128/240-byte leaks after
Lavapipe unload and no report when the loader retains that same driver. A
dependency cleanup correction remains open; its results and boundaries are
recorded separately from these older failed runs.

Observed runs, 2026-10-20:
[39468622435](https://github.com/Lulus792/NexaUI/actions/runs/39468622435)
(documentation source0739c495, production unchanged fromd06fc38), and
[39473369268](https://github.com/Lulus792/NexaUI/actions/runs/39473369268)
(text source2f4199e). The Linux sanitizer jobs remain **failed**. Their six
ordinary linkage jobs pass. This follow-up records allocation sites; it does
not claim a leak repair or change a test result.

## Actual controls and module identity

Both fixed native-clear controls in run39468622435 complete rendering, then
fail LeakSanitizer with240 bytes: one direct128-byte allocation and two
indirect56-byte allocations. Explicit Khronos/synchronization validation is
enabled in one control and removed in the other. Loader output confirms the
validation library is present only in the enabled control. Both still leak.

`LD_DEBUG=files` and the slow allocation unwinder retain module addresses in
the same processes: PIDs8134 and8158. Each application's own loader history
places the allocation frames uniquely inside `libvulkan_lvp.so`. The analysis
keeps histories separated by PID, including the symbolizer's separate loader
output. It does not guess a module base from another process's ASLR addresses.

The Ubuntu apt log installs `mesa-vulkan-drivers25.2.8-0ubuntu0.24.04.4`.
The official matching binary/debug packages were downloaded as offline
diagnostic data, with byte counts and SHA-256 hashes retained; nothing was
installed into Windows or the shared Alpine system. The package library's
load extent0xb8c100 matches both CI loader records. Its binary and debug
Build ID agree: `b3fb0cf7870efc6646e21939393d3d2ad05cf1dc`.

Package-side symbolization gives:

| Allocation | Module offset | Symbol and path |
| --- | --- | --- |
| Direct128 bytes, `realloc` | 0x150de3 | `get_cpu_topology`, called by `_util_cpu_detect_once` and `llvmpipe_create_screen` |
| Indirect56 bytes, `calloc` | 0x4504b1 | `u_mmInit`, then `rtasm_exec_malloc` and the SSE2 vertex-emission translation path |
| Indirect56 bytes, `calloc` | 0x45048d | `u_mmInit`, same translation/execution path |

The indirect stacks continue through `draw_vbo`, `llvmpipe_draw_vbo`,
`lvp_execute_cmd_buffer` and `lvp_queue_submit`, on the driver's submit thread.
The package's function-symbol ranges independently identify these offsets.
`addr2line` also resolves the allocation lines in `u_cpu_detect.c` and `u_mm.c`,
but reports missing DWARF alternate-reference data; its incomplete names are
not presented as a clean full source-level unwind.

The CI artifact does not include the runtime driver ELF's own hash/Build ID.
The exact package version, matching mapped extent and symbol ranges support
this package-side attribution, with that remaining identity limit. The
[machine receipt](vulkan-lsan-module-local.json) retains addresses, PIDs, maps,
package/debug hashes, symbols, diagnostics and the source artifact ID.

After extraction, two byte-identical compressed-member derivatives were
removed only after comparison with their retained `.deb`/`.ddeb` archives,
freeing115,030,733 bytes. The official archives, selected ELF/debug files and
failed/successful diagnostic evidence remain available offline. No build tree,
OS dependency or WSL disk was cleared.

## Independent API evidence and boundaries

The existing `tests/test_vulkan_validation.c` executable links only
`Vulkan::Vulkan`, with no NexaUI source/library. It creates and destroys its
Vulkan objects, exercises the controlled invalid-fence validation case, then
also reports a128-byte leak in these red runs. This is independent evidence
that the startup leak can occur without the framework. It is not a separate
pure-Vulkan reproduction of the drawing path's two56-byte allocations.

The earlier run39464199758 had indirect56-byte reports without retained
same-process loader maps. Those older unknown frames are not retroactively
assigned by matching a different process's addresses. A successful subsequent
run did not repair them. The renewed reports and driver allocation sites now
give a concrete next investigation boundary; the complete lifetime/cleanup
cause and corrective dependency change remain open.

All leak checks, validation, original assertions and time limits stay enabled.
No driver-retention trick, suppression, skip, cache/OS/security setting change
or run-until-green campaign supplies an acceptance result. R03's independent
CPU text comparisons and local Linux117/117 native sanitized run remain
separate evidence. Neither this report nor those passes close the red Ubuntu
sanitizer job, WSLg FIFO liveness, R01 startup, physical input/mixed-DPI or human
assistive acceptance.
