# Original Aetheri rename review experiment

This standalone C17 scene borrows an application-owned review state, a prepared
Aetheri rename operation and an application-owned font collection. The scene presents exact source and
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

At that base this was a bounded headless composition, not a modal, native window, full Files
UI, restart recovery, asynchronous service or Aetheri adoption. Independent
native assistive clients, long-name scrolling, localizations, visual and human
accessibility acceptance, real Aetheri integration and more failure paths remain
open. Aetheri's same-user source check is not a compare-and-swap or durable
identity guarantee; the view does not strengthen that original contract.

## Modal scope and pending confirmation

On base `8e9e0cc4d4b6296fd0757e327f5f5c7ef8daea53`, the existing NexaUI
modal API blocks a representative background action. Scene tests check the
dialog role, blocked pointer and semantic background actions, Tab/Shift+Tab,
Cancel and Escape, and focus returning to that background on dismissal.

An app-owned `rename_review_owner` now retains the confirmation latch while
views are destroyed and recreated. After one confirmation request, Confirm
stays disabled and the view says it is awaiting a result even before the
original operation changes state. The caller must initialize that owner once
for each operation, retain it during view replacement, and never reinitialize
it to retry a pending or unknown operation. The view still does not dispatch.
Tests recreate the view both before dispatch and after an injected unknown
result; mutants removing the latch or resetting it on recreation fail. Static,
shared and ASan/UBSan tests passed, and a 400x300 headless capture was
inspected. This is an in-process, in-memory example; native input and assistive
clients, real async work, durable recovery and Aetheri integration remain open.

## Private Wayland and independent AT-SPI

On base `55eca9d05604d31dbf81272bfd0952c728709967`, a separate test host
renders the review in an ordinary Wayland software window. An independent
libatspi client uses a private registry to discover the modal, checks that the
background is disabled, invokes Confirm, and observes an injected flush error
after the original Aetheri rename. The host retires the old accessibility
bridge, recreates the view using the same owner and operation, and the client
rediscovers the unknown status and disabled Confirm before activating Cancel.
The fixture verifies the original temporary file's new name and exactly one
rename and flush call. A mutant leaving Confirm enabled after the unknown
result fails the client assertion. Static, shared and ASan/UBSan variants passed.

The host runs only in the private Wayland fixture and injects the error; it is
not the Aetheri Files app or a production async service. Its fixed 400x300
layout and 50 ms polling loop are test scaffolding, not resize or idle-power
evidence. Native keyboard, long names, physical input, human screenreader
review, other compositors, real failures and restart recovery remain open.

With the private Weston environment described in the Files example, build this
example and run as an unprivileged user:

```sh
NEXAUI_RENAME_FONT=/path/to/font.ttf xvfb-run -a python3 tests/run_weston_native.py \
  --work-dir /tmp/new-rename-atspi -- python3 tests/run_atspi_native.py \
  /path/to/at-spi2-registryd /tmp/rename-review/aetheri_rename_atspi_client \
  /tmp/rename-review/aetheri_rename_native
```
