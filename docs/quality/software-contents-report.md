# Software manifest file/action inspection — 2026-11-07

Consumer source: `de18e96e57a3bd0cbdedf2042fbfe1eec8ed4398`.
The published checkpoint `da8a008856423af6d4d884590ebae0b7d2fe65be` now has
seven required CI job successes and two optional skips; [exact-head API receipt](software-contents-ci.json).
Later data foundation: [historical process observations](software-history-report.md)
on `ddf4981`; this adds no current process or ownership acceptance to the results below.
Original Aetheri: `46e2a21ef979d9a76db2a8487d1de167a6a79ef7`.
Installed SDK: `c4f467d85b0147944d2456f5e7602a39ac18e579`, NexaUI 0.1.23.
Scoped requirements: D07/L04/L05/L06/L11/I01/I05/T11/A02/A06/P04/P05/P07/Q02/Q04/Q08.

[Example34](../../examples/34_aetheri_software/README.md) now opens a matching
manifest's complete declared file/action tree from installation details. This
continues the [exact observation join](software-components-report.md). It reuses
the installed tree, scroll, focus, action, readonly Text and text-wrap APIs.
There is no SDK implementation, public API, ABI, version or consumer-pin change.

## Behavior and lifetimes

Files retain complete original paths, octal modes, byte sizes and SHA-256 values.
Actions retain the original executable, argument count, all literal UTF-8
arguments and their individual byte lengths. Selecting an action inspects its
declaration; it never resolves/executes a command or opens a declared payload.
Installed contents, ownership, permissions and running-process state remain
explicitly unknown. Group counts describe this verified manifest, not inventory.
Missing/unavailable metadata disables Contents rather than fabricating emptiness.

The two groups expand/collapse through existing tree controls. Up/Down/Home/End
reach all entries. Left from a leaf returns to its group; Left/Right collapse/
expand it. Enter, pointer activation and accessible Select share the same
inspection transition. PageUp/PageDown read overheight rows and detail text.
Back/Escape returns to the exact entry, then Contents, then the full installation
identity. Small-window Close and restored content focus retain their existing
behavior. Theme tokens and reduced-motion settings remain in use.

Entry ordinals are local to the exact immutable manifest/context. They are not
global file IDs. Observation or manifest replacement retires the semantic context,
clears nested entry selection and returns to installation details. A queued old
entry action cannot select a replacement. The owned batch still outlives the
view's binding; worker/owner responsibilities and filesystem deadline limits are
unchanged. The tree caches at most 42 measured rows; partial layout failure clears
that partial cache. Text layout reserves indentation, scrollbar and label insets.
This is a bounded example, not a complete dependency/ownership browser or resource
budget.

## Executed checks

Static Release, shared Release and GCC ASan/UBSan each pass **5/5 CTests** against
the installed SDK. Original Root v2/v10 component cases retain byte-identical
Root/User databases and rows. The full original model/worker and synthetic 100,000-
target regressions remain. New checks inspect every one of 32 paths and eight
actions, all eight 256-byte arguments of the final action, full 256-byte path,
declared 16 MiB size/mode/digest, scroll endpoints, groups, focus return, compact
restore and queued-action retirement. No 16 MiB payload is created or read.

Seven ordinary native Wayland/independent libatspi cases pass ten main checkpoints
each. Three matching-manifest cases additionally pass **84 native content
commands each**, including all 32 path/eight action names, actual keyboard
selection, full readonly metadata, end-of-detail geometry, focus and retired
references. They use German/32px/high contrast, a 360×300 logical window and the
controlled IME. Three identity-failure cases and the default CLI regression
require disabled Contents. All hosts/peers exit 0 without forced kills, only
startup and explicit Read submit original observations, databases remain unchanged,
and shared runtime maps resolve the selected SDK. Maps exclude SDL/X11/EGL/Vulkan
presentation. These virtual inputs and OS peers do not establish physical display,
installed-IME, spoken-screenreader or performance acceptance.

The final component runs produce **108 PPMs**. Eight losslessly converted PNGs
were inspected: [ordinary tree](software-contents-images/800-contents-top.png),
[large tree](software-contents-images/360-contents-top.png),
[last action](software-contents-images/360-contents-end.png),
[path end](software-contents-images/360-path-end.png),
[file start](software-contents-images/360-file-top.png),
[file end](software-contents-images/360-file-bottom.png),
[action start](software-contents-images/360-action-top.png) and
[action end](software-contents-images/360-action-bottom.png).
The edges complement semantic argument checks; they are not images of every
intermediate argument. Borrowed Liberation Sans is hash-bound, not redistributed.

## Retained failures and exact input binding

The first fixture exceeded the original encoder's 16 MiB per-file bound; it was
corrected to that bound. The first keyboard helper sent Enter-down only; adding
the actual key-up exercises ordinary button activation. Neither change relaxes
the original format or product keyboard contract. Review subsequently corrected
the tree's label-width reservation; all final product checks use that correction.

Initial builds and a later sanitizer capture hit the VM's existing tmpfs user
quota. No quota, mount or system-service configuration was changed. Eight older build
duplicates were compared file-for-file against the already verified dependency
archive before their VM copies were retired. Five initial build directories
were streamed into a separate host archive, fully verified and compared again
with live contents before retiring those VM copies. Their sources/logs remain.
The final sanitizer rerun passes all five tests; the failed quota run remains
recorded. `strace` was unavailable, which is recorded rather than claimed used.

Two initial native peers reached the final file row logically but failed to find
it after a context replacement. The final peer reuses the existing bounded
fresh-application discovery before checking the native focused state. The same
product binaries then pass; no SDK focus fix or underlying libatspi cache cause
is inferred. Unexpected peer exit now reports immediately in that command loop.

The [source binding](software-contents-source-binding.json) verifies all **52**
archived inputs against the exact commit, normalizing CRLF to LF only. CTest/
product binaries use input6; the final native peer/driver use input8. Exactly two
files differ: the independent peer helper and native test driver. All product
code is identical. Input4's scratch tree/builds were rebuilt in place from
retained input6, with an explicit before/after source receipt; initial input4
binaries are not claimed preserved. All source exports remain. The
[local receipt](software-contents-local.json) records commands, binaries, source
differences, original inputs, all native results/maps and final images.

## Frozen artifacts

`D:/NexaUI/out/nexaui-software-contents-20261107.tar.gz`:
31,149,685 bytes; SHA-256
`274d79b7662db62ec74b556c25ffdfe47396361f5bf7ede174ac57ac2eb73a0d`.
The full verifier checked all 2,048 regular-file hashes/sizes/modes, 17 symlink
targets and 496 directory modes. Fifteen FIFO fixtures are explicitly excluded
and recreated by the retained runners. The borrowed font and live archive log/
holder journal are outside the boundary. See [archive receipt](software-contents-archive.json).

Initial build archive `D:/NexaUI/out/nexaui-software-contents-attempts-20261107.tar.gz`:
22,804,594 bytes; SHA-256
`a8a77be15a84839255c4424b0be0e539ea18ee300c3a10acd7283bd2c610df59`.
Its full verifier checked 839 files, three symlinks and 404 directories;
three FIFOs are explicitly excluded. See [retirement/verification receipt](software-contents-attempts-archive.json).
Original sources, SDK sources and compositor/OS-peer dependencies remain in the
previous verified [manifest/dependency artifact](software-components-report.md).
The new archive also retains the three SDK installation prefixes.

## Remaining work

Actual inventory, dependency/ownership graphs, process/resource/device/permission
observations and trusted operation admission still require their owner contracts.
The tree groups exact declared full paths; it does not invent directory ownership
or inspect installed payloads. Native held-source-close/wake, foreign clipboard,
extended resize/output migration and broader failure/resource campaigns remain.
Zero-action/empty-argument native variants, additional languages/fonts, other
platforms and physical/human/performance acceptance are not added by this step.
The rest of packet 6 and packets 7–8 remain active across all 103 requirements.
The independent Aetheri checkout was observed at
`3026d4deaf41f66819eb1162a1407065ec8f5dfc` with its existing edits; this work
does not modify that checkout or its SDK pin. No 1.0 release is declared.
