# Reserved Wayland image backing — 2026-11-07

Implementation: `c4f467d85b0147944d2456f5e7602a39ac18e579`, SDK 0.1.23.
Requirements: scoped W01/R03/P04/P05/Q02. This closes the identified sparse-file
backing defect, not every allocation or platform acceptance requirement.

An ordinary native Recovery startup on the Fedora carrier terminated with
SIGBUS before its first application frame. Both ordinary-window background/
software buffers and the borrowed-surface presenter previously created an
unlinked `/tmp` file, extended it with `ftruncate`, then wrote its mapping.
A separate child reproduces SIGBUS with that same sequence. A separate 4 MiB
`posix_fallocate` probe returns EDQUOT (122); global free space does not establish
the user's available quota. The failed native startup and probes are retained.

Both presenters now use one private helper. It creates an anonymous Linux
`memfd` with close-on-exec, reserves its complete backing with `posix_fallocate`
before mapping, retries an interrupted reservation and closes the FD on error.
Only ENOSYS selects the old-kernel unlinked `/tmp` path, also close-on-exec and
fully reserved. Other creation errors are preserved. Reservation errors return
the existing platform error before pixel writes or buffer submission. No public
API, buffer count, pixel format or ownership contract changes.

This follows the documented [anonymous-file contract](https://man7.org/linux/man-pages/man2/memfd_create.2.html)
and [reservation contract](https://man7.org/linux/man-pages/man3/posix_fallocate.3.html).
It is not a guarantee against global OOM termination, a malicious compositor
truncating shared memory, or every possible platform failure.

## Executed checks

Fedora 44 / Linux 6.19.10, GCC 16.2.1, CMake 4.3.0, Python 3.14.3. Wayland-only
builds use X11 OFF, optional Vulkan OFF and examples OFF. Final builds include
the existing runtime-optional AT-SPI implementation with real D-Bus headers.
Release static, Release shared and Debug ASan/UBSan each pass **12/12** targeted
CTest cases with no skips: eight fractional-scale variants, backing failures,
ordinary software presentation, borrowed-surface presentation and management.
The sanitizer profile enables leak detection without exclusions or suppression.

The backing check writes and checks complete 1,920,000-byte mappings, verifies
size/CLOEXEC and descriptor balance, exercises interrupted reservation, the
old-kernel fallback and EDQUOT/ENOSPC/ENOMEM/EOPNOTSUPP/EMFILE/unlink failures.
Fallback temporary-file placement is isolated by the test wrapper; the test
does not claim an actual old-kernel run. A private mutation that ignores only
EDQUOT fails at the expected refusal assertion. The same helper additionally
compiles and passes under strict C17 on Alpine/musl with conversion/shadow
warnings treated as errors.

Four [native Recovery workflows](recovery-native-report.md) also pass on this
SDK while the independent `/tmp` reservation still returns EDQUOT. Each host's
actual process maps contains `/memfd:nexaui-wayland`; shared SDK provenance is
checked from its loaded path. These are virtual-output/software results, not
physical GPU, performance or production-compositor acceptance.

The [machine receipt](recovery-native-local.json) retains commands, tests,
libraries and source bindings. All 1,114 exported SDK files were compared with
the implementation revision: after only CRLF normalization, the sole content
difference is the subsequently added rendering documentation. Original physical
hashes and both source archives remain available.

Earlier configuration/peer dependency failures and a CTest selector that also
matched an unbuilt unrelated executable remain failed records. The corrected
bounded selector passes all twelve intended cases. Earlier builds without the
optional D-Bus headers pass the same rendering tests, but cannot supply native
AT-SPI; their native publication failure is not counted as an accepted workflow.
