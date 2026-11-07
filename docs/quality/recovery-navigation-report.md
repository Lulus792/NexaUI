# Native Recovery keyboard and filter cancellation — 2026-11-07

OS peer/driver source: `d098be32eb28f985cc3ac4a626c5fbd81ff19abe`.
Reused hosts: `1c8fa04942280c2189abbbe2a77dbe06f2f63c09`.
Installed SDK: `c4f467d85b0147944d2456f5e7602a39ac18e579`, 0.1.23.
Original Aetheri: `46e2a21ef979d9a76db2a8487d1de167a6a79ef7`.
Scoped requirements: D11/L04/L11/I01/I07/T11/A02/A06/P05/Q02/Q04.

[Example 33](../../examples/33_aetheri_recovery/README.md) adds `--navigation`
to its existing ordinary native driver and independent libatspi peer. The
ordinary/held host, worker, view/model, SDK and original backend bodies remain
unchanged. The exact prior static/shared/sanitizer host binaries are reused,
hash-checked and copied into the new artifact. No new core CTest run or SDK build
is claimed; their prior [three-profile checks](recovery-lifecycle-report.md)
remain attributed to that source. The OS peer is compiled under strict C17,
`-Wall -Wextra -Wpedantic -Werror -O2`, against real private OS headers/runtimes.
It links no NexaUI code or headers. Optional system pkg-config discovery remains
outside this direct-build evidence.

## Exercised native workflow

The original ten checkpoints remain: full original records, complete read-only
details and native paging, retired references, original refresh, literal and IME
input, local filtering and selection restoration. The extension adds fifteen:

1. Shift-Tab from Close reaches Read; Enter dispatches and completes one actual
   original worker read. End reaches the same selected full record identity in
   the fresh observation. The independent peer rejects the retired old row and
   confirms the new selected row before setting a known toolbar start focus.
2. Native Tab/Shift-Tab checks toolbar focus and opens Filter with Enter. Native
   typing creates a draft `x`, while applied results remain unchanged.
3. Tab and Shift-Tab traverse the field, optional help scrollbar, Apply, Close
   and Back. The independent peer checks native focused states at each stop.
4. Enter on Back cancels to the full result and restores Filter focus. The old
   field refuses late editing; the same full record remains selected.
5. Enter reopens the retained draft. Escape cancels with the same result/focus
   and stale-reference checks; reopening confirms the draft again. A final
   Escape returns before native keyboard Close.

The public [scroll-area descriptor](../../include/nexaui/nexaui.h) explicitly
defines nonzero scrollbar IDs as focusable. Large/narrow filter help has such a
scrollbar; ordinary layouts do not. The OS peer inventories it with a bounded
complete lookup, validates its native role/focusability and separately checks
forward/reverse focus through it. Both paths are asserted, including the expected
presence for the declared large/narrow cases. This is not a skip over arbitrary
unexpected controls or a claim that keyboard scroll-value behavior was tested.

Each navigation case has exactly **three original reads**: initial, existing
OS-action refresh and the additional Enter refresh. Filter typing, Back and
Escape cause no backend work. The driver checks the actual view's complete count
and unchanged original database bytes/rows; the independent OS peer checks the
selected full kind/ID, focus, editable draft and stale edit refusal. The three
Cancel-return checkpoints all have complete record count, Filter focus and
request count three. These remain owner observations, not recovery admission,
external-holder drain or global readiness.

## Executed cases and retained failures

The same Fedora 44 / Linux 6.19.10 x86-64 carrier and installed SDKs are used.
**Six navigation workflows pass, 25 checkpoints each:**

| Reused host profile | Root v10 | User v8 |
| --- | --- | --- |
| Release static | German, 32px, 360×300, contrast, native IME, help scrollbar | English, ordinary/light, no controlled IME, no help scrollbar |
| Release shared | Same Root case | Same User case |
| Debug ASan/UBSan | Same Root case | Same User case |

All hosts/peers exit normally, original bytes/rows remain identical and no forced
cleanup occurs. ASan/UBSan/leak settings remain enabled for the reused selected
host/SDK paths, with no suppressed diagnostics; the OS peer/system libraries are
not all instrumented. Three regressions through the extended driver also pass:
ordinary Release/German/narrow/IME without `--navigation` (10 checkpoints),
Release held-close and ASan/User held-wake (three checkpoints each).

Two isolated view defects compile and fail intended native checkpoints:

- Clearing Shift from forwarded Tab events fails reverse toolbar navigation at
  checkpoint ten.
- Applying the draft on Back instead of cancelling fails complete-result/focus
  restoration at checkpoint twenty.

Each fails its existing seven-second boundary. The ordinary six-case matrix
passes without weakening that boundary. The first exploratory large/narrow run
failed at a mistaken direct-field-to-Apply expectation: actual focus was on its
documented scrollbar. Its sources/binary/logs remain retained. The final probe
checks that scrollbar explicitly; production focus behavior is unchanged.

All 24 exported consumer/common/peer files bind to the exact source after only
CRLF normalization. Only README documentation changed after the native runs;
the final binding verifies every code byte. The [machine receipt](recovery-navigation-local.json)
contains source/binary hashes, exact reused-host bindings, every final checkpoint,
original-data identity and counterexamples; the [archive receipt](recovery-navigation-archive.json)
records full local artifact verification. The prior lifecycle archive is retained
under its original hash.

The local `D:/NexaUI/out/nexaui-recovery-navigation-20261107.tar.gz` contains
13,052,489 bytes; SHA-256
`85515fb621fdb765de0c78b6aae31b513e9bedcfa2d828942e4081d1ef6f5ee8`.
Independent verification checks all 946 file hashes/sizes/modes, two link
targets and 258 directory modes. Source snapshots, six exact reused host
binaries, installed SDKs, original fixtures, commands and failed attempts are
included. The collector's own terminal result is outside the frozen boundary.

The previous main `b0ac871` has all seven required CI jobs successful and two
optional skips; see the [exact-run receipt](recovery-lifecycle-ci.json). These
optional native consumer workflows are independently executed local evidence.
No later CI result is inferred from that prior run.

## Remaining scope

Selected native Tab/Shift-Tab, Read activation, filter Back/Escape, draft/selection
retention and retired-field gates now have direct evidence. Native resize and
compact restoration, output migration and clipboard peers remain open. Human
screenreader, physical display/input, broader localization and performance checks
remain separate from these virtual-input/OS-query workflows. Generic offscreen
assistive realization is not established by the mounted-row checks.

Production authority/service transport, reconnect/unknown outcomes, actual
recovery/readiness and remaining device/software/component/installer/trusted
packet 6 compositions remain open, along with packets 7–8 and the rest of the
103-ID catalogue. Aetheri's live source, catalogue and consumer pin are unchanged.
No 1.0 release is declared.
