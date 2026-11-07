# Tree expansion input targeting — 7 November 2026

Code and regression source:
[`bae657a4cdaaed6e4f6dc426b71ad17c69586c12`](https://github.com/Lulus792/NexaUI/commit/bae657a4cdaaed6e4f6dc426b71ad17c69586c12).
Base: `e1571a5854b1a32dfa8dc50ca6c3d1d53db6aec2`.
This advances L06/I01/Q02 within the existing Aetheri catalogue.

## Finding and bounded correction

`nx_tree_item` previously applied every Left/Right key in a frame to whichever
branch or expander had focus when the view was rebuilt. For example, Right on
folder A followed by a click on folder B opened B and left A closed. A Tab focus
change or an application focus change during frame construction could similarly
retarget an earlier key.

The control now checks the existing per-event routed focus ID against its row
and expander IDs. Earlier input stays with its original target even when focus
subsequently moves. The production diff is confined to that loop. It adds no
allocation, public API, ABI, dependency or version change. Aetheri source,
application models, current SDK selection and frozen evidence remain untouched.

## Executed evidence

Three external C17 consumers link independently installed SDKs: static Release,
shared Release and static Debug with ASan/UBSan/leak detection. All SDKs and
consumers build with `-Wall -Wextra -Wpedantic -Werror`; the SDK builds emit no
warning or error diagnostics. All three execute the complete tree regression.

Nine added cases check model expansion, row/expander semantic expansion, final
focus and selection. They cover a row key followed by a pointer focus change,
an expander key followed by that change, the reversed order, collapse, two Tab
steps with keys on separate branches, a disabled target, consumed key input,
application focus movement during composition and a second-expander action.
These are public-event core checks; they do not claim native keyboard or
independent screenreader execution.

The original three installed SDKs each reject the first new case with
`expanded 0/1, expected 1/0`. Those binaries come from `0d36408c120bd3ec59747688fa0ae6e94fc2dfe6`;
the production SDK inputs are unchanged between that source and the base above
(only documentation and the native-management test differ). Their original
artifact provenance remains in the [strict SDK report](strict-c17-sdk-report.md).
The initial sanitizer negative returned through the test assertion before
context destruction and consequently reported retained context memory. That
failed-test cleanup was corrected; a separate final-test negative still rejects
the same baseline behavior and has no sanitizer diagnostic. Both versions/logs
are retained. The successful candidate uses the final cleanup-safe regression.

Regular root builds use their normal project warning flags, not the additional
SDK-wide Werror probe. All are Wayland-only software profiles with AT-SPI enabled;
Vulkan and examples are disabled for this check.

| Root profile | Passed | Registered | Skipped |
| --- | ---: | ---: | ---: |
| Static Release | 144 | 150 | 6 |
| Shared Release | 143 | 149 | 6 |
| Static Debug ASan/UBSan/LSan | 144 | 150 | 6 |

The shared suite omits the static-only selection-set case. Each suite executes
the existing tree flattening, allocation-failure/retry, interaction, selection,
tree-in-table and headless pixel assertions as well as the other registered
core/protocol tests. The six skipped cases are external-display window,
poll-fd, native management, group polling, reentry and peer-destruction tests.
They remain skips; this step adds no separate native compositor campaign.
No warning, assertion, timeout or sanitizer setting was relaxed.

Environment: Fedora 44 x86-64, Linux 6.19.10-300.fc44, GCC 16.2.1, CMake 4.3.0,
Python 3.14.3, Wayland-client 1.26.0 and xkbcommon 1.13.1. Release uses `-O3 -DNDEBUG`;
the sanitizer profile uses Debug and `-fsanitize=address,undefined
-fno-omit-frame-pointer`, with leak detection and halt-on-error enabled.

## Artifacts and remaining requirements

`D:/NexaUI/out/nexaui-tree-routing-20261107.tar.gz`, SHA-256:
`1cad914f123f1738866c59b231171058cb1b3d97b19603eda7319f0584c62841`.
All 7,262 manifest files and eight symbolic links were independently verified
after transfer to the local host.

The evidence archive retains exact sources, input archives, before/after installed
SDKs and external consumers, full root builds, command logs, JUnit and file hashes.
All 1,013 source files match the code revision, allowing CRLF/LF normalization
for copied text inputs. Guest and host clocks differ; only copied source mtimes
were normalized. The system clocks and existing evidence were preserved.

Full tree directional navigation, breadcrumbs, asynchronous child-loading/error
states, component inventories and real picker/service integration remain open.
This change also leaves the unpaced Files sanitizer burst, held-pointer loss,
other platform/native acceptance, installed IMEs and human/physical gates open.
There is no performance or total-memory comparison in this evidence. The SDK
remains development version 0.1.23; 1.0.0 still requires the user's approval.
