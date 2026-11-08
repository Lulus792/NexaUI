# Files native IME transactions and undo — 2026-11-08

Implementation: `0a1f806c2de57ca864b59369f9aefd94cbe8038e`.
The separate, default-OFF Files IME variant now consumes the installed SDK's
complete native transaction envelopes. Original filename/query validation,
filesystem controller and operation/unknown-outcome policy retain their owners.
This is packet 6 progress, not complete Files, app migration or final adoption.

Affected catalogue evidence: B01/B03/B04, I01/I02, T01/T03/T04/T10,
A02/A04, P04/P05, Q02/Q03/Q06/Q08. All 103 catalogue IDs and the full remaining
packets 6–8 remain in scope. Catalogue SHA-256:
`9c932872caeba8007bf05b86f7c167b493260709b23c54d709222a5f19539e65`.

## Sources and independent ownership

The app export is Aetheri `7483286bcaafbeaced5831f72cb4d0e0cad9a158`, with
87 checked inputs. The private producer separately exports 17 inputs from
`d56d69523c33a48648af343e7d36526f84518c90`. All 104 original inputs have been
compared to their Git blobs. The consumer generator produces three app files;
the producer generates two fixture C files. Reversing their recorded changes
reproduces the originals in each accepted build. Neither export is modified.

The installed SDK is `ca1cb3db26badde2c6fbe3cc1bae1de1b149ad18`, version
0.1.25. Its 1397 source-archive files match Git blobs and the actual VM source;
42 installed package files are bound separately. Its public headers match the
frozen source. This checkpoint changes no SDK source, API, ABI or version.
The previous SDK's [atomic widget checkpoint](ime-atomic-report.md) remains
separate; its exact-main [CI metadata](ime-atomic-ci.json) does not execute
this optional external Files consumer.

A read-only committed-source survey of Aetheri
`4e6141da2c3f46827402c78129daab358dbadf2b` finds only `CMakeLists.txt`
different among the 87 selected app inputs. Its reviewed SDK pin remains
`97a42647480bb05167cee5e4ae08cedd9147d80d` / 0.1.23. This is no runtime
test of that live checkout. Its source, builds, backend and pin are untouched.

## Transaction and lifetime behavior

The generated host opts into native transaction boundaries before its first
IME event. A 263-slot app event buffer holds a complete envelope, validates its
current focus/target/revision/scope and forms the whole bounded byte proposal.
The original filename/query rule then accepts or rejects that proposal. Only
accepted complete envelopes enter the real SDK field. Each field now has a
bounded 32 KiB undo history; deletion/replacement/empty commit are one edit,
including restoration of reversed selection. Two complete packets keep two undo
entries even when delivered before one frame.

A temporary public text buffer tracks the predicted text and grapheme-aligned
caret across coqueued packets. The candidate is built as one combined byte
splice: separately deleting and then inserting could snap the intermediate
caret across a newly joined combining cluster. The preview never changes the
original app model. It is freed at frame entry, replacement and view destruction.
The old valid draft is retained on rejection and the original invalid state
blocks confirmation; a valid first packet is applied before rejecting a second.

Incomplete/malformed/oversized/nested packets never enter the field. A changed
scope or valid newer target/revision can retire an incomplete packet; ordinary
interruption rejects and clears it. Late fragments cannot complete an old edit.
An explicit clear, model refresh or focus/selection change permits fresh input.
The host drains native payloads before drawing and flushes completed edits before
ordinary app commands. Existing preedit cancellation and command suppression
remain, including Escape, Tab and close with active preedit.

## Executed checks

See [local evidence](files-ime-atomic-local.json) and
[source/artifact binding](files-ime-atomic-source-binding.json).

- Fresh shared Release, static Release and static ASan/UBSan/LSan consumers each
  pass **24/24 CTests**, with **48 explicit C17/Werror objects per profile**.
  They include unchanged original model/session/document/child tests, original
  and generated 1×/2× views and the new ten-case controller/parser test.
- The parser test uses real public host-IME tokens with a substituted native
  guard. It covers split delivery, nested/wrong/missing endings, overflow,
  stale replies, model refresh, focus and selection revisions. It is explicitly
  not native-window evidence. All ten cases pass on each final profile.
- The independent native Wayland/AT-SPI task passes **188 checks and 39 virtual
  captures per profile**. It checks Unicode/delete/replacement/empty and rejected
  proposals; byte versus scalar offsets; reversed selection; one-packet undo;
  separate undo/redo for two packets; combining-cluster caret behavior; a valid
  first plus invalid second packet; late commits after Escape/Tab; 200% text;
  real original create and rename; and app-driven close with active preedit.
  Each app exits 0 with `failed=0`; fixture seed bytes remain intact.
- Independent libatspi peers link neither NexaUI nor Files. They read committed
  text, scalar caret and selection and observe disabled confirmation. Shared
  app mappings contain the exact selected SDK; all Files mappings exclude SDL,
  X11, Vulkan and EGL. The separate original Terminal remains an SDL child.
- A fresh default-OFF 0.1.23 shared consumer passes **3/3 CTests**. IME ON with
  0.1.23, default OFF with 0.1.25, and a wrong SDK source declaration each fail
  their specific configure guard. Existing test deadlines/assertions are kept.

Two genuine negative controls are retained. The preceding controller version
passes cases 0–3 but fails case 4 after a missing END followed by explicit clear;
the correction passes all ten. A separately compiled wrong-preview-caret variant
runs the identical native task and exits 1 at the second coqueued invalid UTF-8
range check (144 prior recorded checks, 27 captures). Accepted sources, binaries
and assertions are unchanged by that control.

Thirteen states on three virtual outputs yield 117 final captures. Twelve
lossless PNG conversions cover six shared-profile states on outputs 0/1; three
output-0 PNGs were visually inspected: 200% preedit, rejected second packet and
rename after separate undo/redo. Preedit/committed text, invalid outline and
disabled confirmation are visible. The inherited shortened Cancel label and
previously observed font-fallback gap remain visual work; these images are no
physical display or human screenreader acceptance.

## Failures, artifact retention and remaining work

Earlier runs and failures are retained: bounded-generator mismatch, test harness
feature-macro/empty-seed errors, the actual incomplete-packet recovery defect,
and initial missing AT-SPI discovery in the separately rebuilt SDK. The latter
was caught by a public runtime-feature probe, then rebuilt with the checked
private D-Bus development fixture. It is not counted as an accepted accessible
SDK build. UID-quota failures (`EDQUOT`) are recorded separately from app defects;
unchanged assertions passed after fully archived obsolete build copies were
retired. The [artifact receipt](files-ime-atomic-archive.json) identifies the
final archive and supporting SDK/intermediate archives. All bytes, sizes, modes,
links and directories are checked; inactive original-test FIFOs are listed but
not serialized. No broad cleanup or active Aetheri modification was performed.

Final host artifact: `D:/NexaUI/out/nexaui-files-ime-atomic-20261108.tar.gz`,
50,190,165 bytes, SHA-256
`d60e90e67e58e5ebc480f6e5658b8b6a500ba8210cdf1ca14c0ce6339620943a`:
3262 files, 19 links and 2173 directories. All 70 explicit artifact rows, all
1397 SDK source files, 42 installed package files and 15 executed consumer
inputs also match this archive's verified manifest. The separate accepted SDK
archive retains its retired build objects and raw tests; supporting archive
identities are listed in the receipt. The [final checks](files-ime-atomic-checks.json)
summarize those bindings. [Reproduction instructions](../../examples/35_aetheri_files_consumer/README.md)
keep the frozen app and producer builds independent.

Open: installed real IME/provider routing and pressure, foreign clipboard,
font fallback/localization, broader consumer fault/admission/resource acceptance,
remaining packet 6 compositions, packets 7–8 content/VM/consumer work, final
performance budgets and separately available physical/human/three-output tests.
Virtual producer input grants no compositor/session authority. Original app/SDK
adoption and all final catalogue gates remain separate. No 1.0 release is made.
