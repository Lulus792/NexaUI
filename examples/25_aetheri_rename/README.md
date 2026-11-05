# Original Aetheri rename review experiment

This standalone C17 scene borrows an application-owned review state, a prepared
Aetheri rename operation and an application-owned font collection. The scene presents exact source and
destination bytes as reversible escaped ASCII, an operation status, Cancel and
Confirm. It returns an intent; it does not call `ae_files_rename_apply`, choose a
directory or grant file access. The owner retains the original operation across
view destruction and may dispatch only after its own checks. Confirm is disabled
after any non-prepared outcome, including unknown or conflict. At the initial
base a name too long for the fixed scene returned `NX_ERROR_LIMIT` instead of
offering a clipped confirmation; the later scrollable version is described below.

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

## Private Wayland keyboard

On base `21abd07873507fa6253a39e4a2b3179f0c45c03f`, the private Weston
virtual-keyboard fixture runs two separate temporary-file sessions. Escape
cancels before any rename or flush. In the second session, Tab and Enter
confirm the original operation, the injected flush failure leaves an unknown
result, and the recreated view keeps Confirm outside the focus sequence;
Enter then closes through Cancel. The fixture verifies both filename/content
outcomes and exact operation counts. A mutant that reenabled Confirm failed
with an explicit focus assertion. Static, shared and ASan/UBSan builds passed;
the independent native AT-SPI regression also passed with the same host.

This exercises virtual keyboard events on one private Weston configuration.
Other keyboard layouts, physical input, native resize, human accessibility,
real asynchronous operation and actual Aetheri integration remain open.

```sh
xvfb-run -a python3 tests/run_weston_native.py --work-dir /tmp/new-rename-key \
  -- python3 examples/25_aetheri_rename/run_keyboard.py \
  /tmp/rename-review/aetheri_rename_native /path/to/font.ttf /path/to/xdotool
```

## Scrollable long-name review

On base `220611c24ce92e4cb6897382681148db4dca91f1`, the 400x300 review uses
the existing NexaUI scroll area for both full names. Its reversible ASCII byte
representation wraps only between complete bytes, never inside `\xNN`. Before
a long preview reaches the bottom, Confirm is disabled and the status asks for
review. After reaching the bottom, a later frame enables confirmation. A new
view starts the visual review again. This ensures the names are available and
the bottom was reached; it cannot prove that a person read each line.

Scene tests cover a 255-byte ASCII destination, a 255-byte non-UTF-8 original
represented as 1,020 escaped characters, disabled confirmation before scrolling,
End-key scrolling and no early filesystem effects. A mutant removing the
pre-scroll gate failed. Private Weston keyboard tests scroll and confirm a
255-byte name; an independent AT-SPI client verifies the full destination,
scrollbar and initially disabled Confirm. Static, shared and ASan/UBSan variants
passed; top and bottom 400x300 captures were inspected. The private host still
uses a fixed-size layout and simulated flush failure. Actual screenreader
scrolling/announcements, proportional fonts beyond the measured layout bound,
localization, real Aetheri integration and physical acceptance remain open.

## Assistive page navigation

On base `7cf7062d81fbaa33d36b5127a711e65855e48b9b`, long reviews also expose
visible `Previous page` and `Next page` button actions. An independent native
AT-SPI client pages to the bottom, observes Confirm become enabled, pages back,
and confirms the same 255-byte operation before checking its injected unknown
result. The completed scroll gate remains satisfied after paging back; it does
not claim the user read the contents. Static, shared and ASan/UBSan scene,
keyboard and AT-SPI variants passed; a mutant that ignored page actions failed.
A 400x300 headless capture was inspected.

The generic scrollbar currently advertises increment/decrement actions, but
the public core action queue does not handle those actions. The explicit page
buttons provide the tested assistive path for this example; they do not close
that reusable scrollbar contract gap. Human screenreader and physical-input
acceptance, other fonts and actual Aetheri integration remain open.
