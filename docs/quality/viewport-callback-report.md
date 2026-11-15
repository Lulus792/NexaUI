# Viewport source callback lifetime

Candidate `529d5a9d102d975f4fa0664df4936d343bc63b58` uses development version
0.1.33. The existing `nx_viewport_source` interface borrows engine-owned RGBA8
pixels until the caller completes the frame lease. This change makes that
lease safe against same-surface calls from the source callbacks.

The previous installed SDK at `1e00a6f4f3a0ab3a7da1aa35a36f02679421366b`
allowed early destruction during resize, acquisition, failed-draw release and
resize to zero. Regular completion could call release a second time from inside
the first release callback. Release and sanitizer builds each rejected the five
original diagnostic cases. A separate minimal acquisition case reported a
heap use after free under AddressSanitizer.

The viewport now tracks whether the surface is idle, in a call or leased to a
frame. Resize, acquire and release callbacks receive `NX_ERROR_STATE` if they
try to draw, complete or destroy the same surface. An independent surface with
its own context can draw and complete on the same thread. The caller keeps the
active context and callback data alive and defers closing until the call returns
and any frame lease is complete. See the [viewport contract](../concepts/viewport.md).

The regression exercises five callback phases, a render and release on an
independent surface, recovery after a malformed bitmap and a successful later
draw on the original surface. Clang 22.1.3 on Alpine-Dev ran **36 SDK CTests**
and **6 installed-consumer CTests** across Wayland-only static Release, shared
Release and Debug `-O1` ASan/UBSan/LSan. Each installed profile ran the full
regression and the small case that previously reported the invalid access.
The initial external harness used CMake's reserved `test` target name. Its
first correction still collided with Ninja's generated `test` rule at the
output path. The final harness gives both the target and its output distinct
names and reuses the already installed libraries.

The [local receipt](viewport-callback-local.json) records exact source, package,
and test binary hashes. The local archive `nexaui-viewport154-evidence.tar.gz`
has 32,043,304 bytes and SHA-256
`0da2655cf529621fcecca77f3651b41cdadd3916e15725659e2f8b81dbe4364e`.
All 81 members were read back and verified. Platform CI runs against the same
implementation commit. [GitHub Actions](https://github.com/Lulus792/NexaUI/actions/runs/43746582349)

This advances R06, R10, P05 and Q02 at the CPU frame lease boundary. Engine frame
admission after tab close, end-to-end input routing, IME, the accessible content
subtree, native presentation and real browser/PDF/guest integrations require
separate evidence.
