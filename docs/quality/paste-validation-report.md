# Optional whole-splice paste admission — 8 November 2026

SDK implementation: `794be7cec49f427fcc56e2604cddb44245347021`, development
version **0.1.26**. This advances B03/B04, T03, I05, A04, P04/P05 and
Q02/Q03/Q06/Q08 within their described scope; no complete catalogue ID is closed.

## Contract and reason

The optional Files0.1.25 consumer currently accepts a widget paste and then
rolls back an app-invalid draft with `nx_text_field_set_text`. That restores text
but resets history. Existing `desc.validate` deliberately reports draft validity;
it is not an admission callback. Wrapping the native clipboard read callback
would also bypass the existing asynchronous native adapter recognition.

The new public `nx_text_field_set_paste_validator` instead copies an optional
function/user pair into the opaque field. It checks a complete proposed splice
before modifying the buffer. The callback borrows current text, replacement
start/end UTF-8 byte offsets and normalized insertion. It can reject an oversized
or app-invalid complete proposal without allocating another combined text copy.

Zero rejects with text, selection, undo and redo preserved and no new undo entry.
There is no automatic invalid flag: application error presentation stays app-owned.
The hook applies to keyboard, logical-command and assistive Paste, including
deferred native completion. Assistive Paste inserts at its requested byte offset.
Typing, IME, Cut, Undo and application/assistive Set retain their existing policy.
The same native transfer, byte budgets and deadlines remain in use. This does not
add a smaller transport limit or establish worst-case memory/performance bounds.

Configure on the owner thread between widget submissions. The callback must not
retain spans or mutate/destroy NexaUI objects. User data outlives callbacks until
removal or field destruction. NULL/NULL removes the policy; NULL with a non-NULL
user returns ARGUMENT. A changed function/user increments the existing model
revision and invalidates pending native/assistive work, even when changed back.
The identical pair is a no-op. Configuration preserves the text and its history.
Default fields have no validator; numeric fields retain their previous behavior.

Public descriptor layouts are unchanged. One public function and callback typedef
are added; package/header version is raised together. See the [contract](../concepts/text.md#admit-a-complete-paste-before-changing-the-draft)
and [installed C17 consumer](../../tests/paste_validator_consumer/CMakeLists.txt).
Consumers using the new symbol must require at least package0.1.26 and a matching
runtime library. Older same-SONAME DSOs do not contain that symbol.

## Executed evidence

The exact committed export was compiled on Alpine-Dev/Alpine WSL x86-64 musl:
GCC15.2 Release static/shared and Clang22.1.3 Debug ASan/UBSan with leak detection.
Wayland1.25.0, protocols1.48 and xkbcommon1.13.1 were used. X11, Vulkan, examples
and AT-SPI transport are explicitly OFF; the controlled native fixture supplies
its own Wayland server. No ordinary desktop clipboard, physical display or
privileged Aetheri service is used.

Each of the three profiles passes **14/14 affected CTests**, with 76 compiled
objects per profile and `-Werror`. New targets explicitly use C17. The suite
includes existing text/numeric fields, text values/ancestry/completion, text
buffer, atomic IME and legacy/marked IME wire cases, and native text/numeric
clipboard regressions. Existing deadlines and sanitizer checks are retained.

The new public-field test exercises keyboard/logical Paste and, on Linux, the
assistive-request API. It checks complete-splice offsets, single/multiline CR/LF,
Unicode insertion, malformed UTF-8/NUL refusal, reversed selection, rejected-paste
undo and an existing redo branch, accepted-paste single undo, policy removal,
invalid arguments and queued-request invalidation versus identical-setting reuse.
Assistive completions report rejection/acceptance and release their lease.

The new native test exercises all three routes through actual Wayland clipboard
FDs. No callback runs synchronously when Paste starts. A held server writer
proves partial-data/no-EOF state. Changing the policy away and back rejects that
pending result without a validation callback; identical reconfiguration permits
the still-current result. Rejected/accepted history and selection are checked,
assistive leases balance, and a fresh public read can start after field cleanup.
These are controlled API/server tests, not a new independent libatspi-client run.

Three separately configured installed C17 consumers build/run successfully.
Actual old package0.1.25 configuration fails the0.1.26 requirement. A new shared
consumer with the actual rebuilt0.1.25 DSO first in its lookup path fails on
`nx_text_field_set_paste_validator: symbol not found`; the lookup confirms that
old path. Two compiled private counterexamples configure/build successfully and
exit1: ignoring rejection, and omitting policy-revision invalidation. Neither
counterexample is an assertion-free copy or a claimed old-SDK runtime behavior.

All1434 exported files bind to committed Git blobs. Windows Git materialized
some authored text as CRLF: raw export hashes remain recorded, and only exact
CRLF-to-LF canonicalization is accepted for those blob comparisons. Binary and
Unicode inputs must match their actual blobs. This is not a claim that every
raw exported byte already equals its LF Git form.

The installed-public-header/export intersection gains only the new function and
loses none. Raw ELF exports are499 before and526 after; the old baseline built
with tests OFF, while new profiles use tests ON. The26 additional private test
hooks are recorded separately rather than attributed to the public API change.
[Local receipt](paste-validation-local.json) retains all command, object, input,
binary, export and negative-case evidence.

## Preserved diagnostics and continuation

Initial private test setup omitted mandatory assistive lease callbacks and used
a nonexistent fixture setup helper; those tests were corrected to use existing
contracts. A native probe initially cancelled a fresh reader before its producer
had processed receive. The retained GDB trace locates SIGPIPE in the fixture's
`offer_receive` write. Waiting for its actual held-writer witness corrects that
probe; SIGPIPE was not ignored or suppressed. One shell regex invocation also
failed before running CTest. The final driver passes argument arrays directly.
The first installed-consumer checker expected musl `ldd` to exit1; its actual
missing-symbol exit127 and output are preserved, and continuation checks the
actual loader rejection without repeating already completed profiles.

Aetheri independently advanced to `141260d0c9e55f75bae89d11267fd4f0c78c89b3`
in the read-only survey, retaining reviewed SDK
`97a42647480bb05167cee5e4ae08cedd9147d80d`/0.1.23. It was not changed or built.
The optional Files experiment still selects0.1.25; adopting this validator there
and proving original whole-proposal rejection/history behavior is the next step.
The previous Files main6209e1a has [seven required CI successes and two optional
skips](files-appearance-ci.json); these are not results for this new SDK revision.

Broader clipboard budgets/pressure/deadlines/owner loss, actual OS assistive and
IME providers, remaining packet6 compositions, packets7-8 and final physical/
human/performance/adoption/three-output acceptance remain open across103 IDs.
Catalogue hash remains `9c932872caeba8007bf05b86f7c167b493260709b23c54d709222a5f19539e65`.
No Aetheri repin, default UI migration or1.0 release is made.

[Archive receipt](paste-validation-archive.json) binds the complete raw source,
builds, installed prefixes, consumers, counterexamples and diagnostic scripts.
The [cross-check receipt](paste-validation-checks.json) verifies all326 explicit
artifact rows and1434 source files against the archive and preserves all103 map
rows unchanged. Of those source inputs,1244 have the documented CRLF export
materialization; binary/Unicode rows are checked as identity exports.

Final archive: `D:/NexaUI/out/nexaui-paste-validation-20261108.tar.gz`,
90,796,640 bytes, SHA-256
`4e57625e9cad1ce6341f0336df1080fdc6cbdc7957fca42c97dd1ddae978d026`:
3621 files,8 links and1752 directories. All file bytes/sizes/modes, directory
modes and link targets are independently verified; no special file is excluded.
