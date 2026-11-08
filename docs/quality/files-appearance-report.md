# Files selected-symbol fallback and complete naming captions

Implementation: `2aaa7aa151471193fd7e15452d2033ed97e3fd8d`, 8 November 2026.
This is a further packet-6 consumer step for T02, L01/L02, D02, P04/P05 and
Q02/Q03/Q06/Q08. It does not close those catalogue requirements in full.

The default-OFF generated Files IME variant reuses the existing public
`nx_font_open`/font-collection APIs. The original Files snapshot remains
`7483286bcaafbeaced5831f72cb4d0e0cad9a158`, with 87 checked app/child inputs;
the private producer remains `d56d69523c33a48648af343e7d36526f84518c90`, with
17 checked inputs and two reversible generated fixture files. The installed SDK
source remains `ca1cb3db26badde2c6fbe3cc1bae1de1b149ad18`, version 0.1.25.
No SDK API, ABI or version change is made. The unchanged default original
consumer still selects its separate 0.1.23 installation.

## Behavior and ownership

The native host accepts at most four explicit `--fallback-font` files, each at
most 4 MiB. The primary retains precedence and its original less-than-64-MiB
file limit. Missing/oversized/unsupported files refuse startup with diagnostics.
There is no global font discovery, installation or automatic download.

The generated application-private `ae_files_view_add_fallback` admits faces
only before the first frame. Faces belong to the view; their original byte
buffers belong to the host and outlive the view. Teardown destroys cached
layouts, fields, context and collection before closing faces, then the host frees
the bytes. This bounds admitted source bytes, not every parsed-font allocation
or the entire application's memory use. Live font replacement remains separate.

The list naming row now measures its full localized Cancel/Abbrechen and OK
captions with the actual app font collection. Buttons retain their minimum
width; the text field gets the remaining space. Grid/review layout, action IDs,
semantic names and original application/controller policies stay in use.

The optional test face is Noto Sans Symbols2 Regular 2.003, pinned to upstream
`notofonts/noto-fonts` commit `be555adc61c20b2240bce79203b1ab1459765171`.
The [input manifest](../../examples/35_aetheri_files_consumer/appearance-font-inputs.json)
records font/license Git blob IDs, sizes and SHA-256 hashes. Both downloaded
inputs independently match their Git blob identities. The OFL-1.1 license and
font remain in the evidence archive, not the Git source or a global font store.
A real public-API probe observes missing U+1F30D in Liberation Sans and glyph908
in the selected fallback; Latin A still resolves to the primary. This supplies
a monochrome globe outline, not color emoji or comprehensive Unicode coverage.

## Executed checks and source binding

Host: existing private unprivileged x86-64 glibc Linux VM, GCC16.2.1, Ninja,
Wayland1.26.0; private pixman/wlroots0.20.2 compositor and independent libatspi
client. No physical display or ordinary desktop seat is exercised.

Three profiles pass **26/26 CTests each**: shared Release, static Release and
static Debug with ASan/UBSan and leak detection enabled. Each profile compiles
49 C17 translation units with `-Werror`. Existing deadlines are retained.

- The new drawn-caption test passes 96 EN/DE cases per profile: list/grid,
  1/1.25/1.5/2 text scale, three window sizes and 1x/2x raster density. It checks
  actual cached draw-layout bytes and finite, positive, nonoverlapping controls.
- Fallback checks cover real font selection, invalid/oversized inputs, four-face
  admission, primary precedence, post-first-frame refusal and teardown lifetime.
- Existing ten controller cases and original Files/Terminal/editor tests pass.
- Each final native task passes 290 checkpoints, including preedit, atomic
  replacement/selection/undo, original create/rename, independent AT-SPI actions,
  foreign Unicode clipboard, immutable copy, rejected complete drafts, late focus
  replies and clean close while a foreign source remains stopped.
- Each profile also checks five native startup refusals: missing argument, fifth
  font, missing file, 4-MiB-plus-one input and malformed face. Expected exits are
  2/2/1/1/1; failed startup writes no fixture file and does not report READY.

Compiled snapshot19 contains the final C/generator/CMake sources. Native
snapshot21 differs only in README and the runner, including startup/error and
capture-content checks. All **20 final example files** bind byte-for-byte to the
implementation commit. All three generated app files reverse exactly to the
frozen original inputs. [Local receipt](files-appearance-local.json) and
[source binding](files-appearance-source-binding.json) retain commands, binary
hashes, distributions of cases and the exact input differences.

The actual new caption test also compiles against the preceding accepted
generated view and unchanged original model/SDK. That executable exits1 on its
first case: the cached rendered caption is `C…`. A first counter-build missed an
original include directory; its configure/build failure remains recorded. Only
the private counter-build include path was corrected before the intended failure
was observed; no production test or timeout was weakened.

There are 153 final virtual output captures, all raw decompressed hashes checked.
Every three-output capture group has nonuniform pixels on at least one output.
The malformed-font startup briefly maps/unmaps the first virtual slot, so the
valid app maps to output1. An initial output0 PNG selection showed only background;
the recorded MAP/UNMAP sequence identified the correct output. The final runner
now rejects an all-background group. Four lossless output1 PNGs are retained;
Unicode/globe, range replacement and rejected-draft images were inspected, with
full Cancel visible. This is no three-output migration or human visual acceptance.

## Adoption and remaining work

The [example instructions](../../examples/35_aetheri_files_consumer/README.md)
describe explicit font paths, lifetimes, optional CTest font and native options.
Aetheri can review this selected-symbol and caption integration independently;
normal app migration/admission and font distribution remain its own decisions.
Read-only Git survey still finds Aetheri HEAD
`aca8235dedc5c2b9c05eb059f8a66d8d225b8d9d` and reviewed SDK pin
`97a42647480bb05167cee5e4ae08cedd9147d80d`. That checkout was not built or changed.
Previous exact main `0029828` has [seven required CI successes and two optional
skips](files-clipboard-ci.json); those jobs do not execute this external consumer.

Broader font/script/locale and runtime font-change acceptance, clipboard full
budget/pressure/deadline/owner-loss and rejection-history policy, production IME
providers, remaining packet6 compositions, packets7-8 and final physical/human/
performance/adoption gates remain open. All 103 catalogue rows and catalogue
SHA-256 `9c932872caeba8007bf05b86f7c167b493260709b23c54d709222a5f19539e65`
are preserved. No 1.0 release is declared.

The [artifact receipt](files-appearance-archive.json) and
[cross-check receipt](files-appearance-checks.json) bind raw inputs, full builds,
native results, fonts/license, failure controls and supporting historical archives.
Owned obsolete build copies and a duplicate SDK input tar were retired only
after matching verified host archives, path/UID/namespace and process guards;
the original extracted sources and installed SDKs remain available.

Final archive: `D:/NexaUI/out/nexaui-files-appearance-20261108.tar.gz`,
59,344,931 bytes, SHA-256
`6554d0557ef00bd78210b2bcbfbe5a60a63a1e2efae018c0c6357e5d14306f48`:
5864 files, 59 links and 3097 directories. Every file hash/size/mode, directory
mode and link target is verified; listed inactive test FIFOs are not serialized.
