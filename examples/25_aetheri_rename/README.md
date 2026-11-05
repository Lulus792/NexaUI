# Original Aetheri rename review experiment

This standalone C17 scene borrows an already prepared Aetheri rename operation
and an application-owned font collection. The scene presents exact source and
destination bytes as reversible escaped ASCII, an operation status, Cancel and
Confirm. It returns an intent; it does not call `ae_files_rename_apply`, choose a
directory or grant file access. The owner retains the original operation across
view destruction and may dispatch only after its own checks. Confirm is disabled
after any non-prepared outcome, including unknown or conflict. A name too long
to display completely in the fixed 400x300 scene returns `NX_ERROR_LIMIT` before
a frame can offer confirmation; it is not silently clipped.

The tested original Files operation comes unchanged from Aetheri revision
`8bfa5684c809899edfc2765e6c03478e2629e9bf`. The scene was developed on NexaUI
base `28827b139e2d407305a1ff5ac4a41c39baf3720c`; the installed static, shared
and ASan/UBSan 0.1.18 SDKs used here were built from
`00649d379c10bc4565daaf63e6faad8db275e2f2`, whose SDK source and build files
match that base. Aetheri's mutable checkout is not changed or pinned by this
experiment. Use an independently archived snapshot, e.g.:

```sh
git -C /path/to/Aetheri archive 8bfa5684c809899edfc2765e6c03478e2629e9bf \
  src/files src/editor src/text | tar -x -C /path/to/aetheri-snapshot
cmake -S examples/25_aetheri_rename -B /tmp/rename-review \
  -DAETHERI_SOURCE_DIR=/path/to/aetheri-snapshot -DCMAKE_PREFIX_PATH=/path/to/installed-nexaui
cmake --build /tmp/rename-review
/tmp/rename-review/aetheri_rename_review_test /path/to/font.ttf
```

Run the test as an ordinary non-root user on a local filesystem. It creates a
private temporary directory, uses the original Aetheri prepare/apply functions,
and checks cancel, explicit keyboard confirmation without early writes, a
successful kernel rename followed by an injected flush failure, unknown state
after view recreation, no replay, destination conflict, byte escaping and
oversized-preview rejection. The test wraps `renameat2`/`fsync` to count calls
and inject the flush failure; it does not claim a production I/O failure was
observed. A mutant that enabled Confirm on the unknown result failed. A 400x300
headless capture was inspected. Static, shared and Clang ASan/UBSan builds
passed as an unprivileged user.

This is a bounded headless composition, not a modal, native window, full Files
UI, restart recovery, asynchronous service or Aetheri adoption. Independent
native assistive clients, long-name scrolling, localizations, visual and human
accessibility acceptance, real Aetheri integration and more failure paths remain
open. Aetheri's same-user source check is not a compare-and-swap or durable
identity guarantee; the view does not strengthen that original contract.
