# Packet 6 system compositions: implemented sources and external seams

Assessment for software-model source `64c9ab101bdaea2ff47e650ea911e81f1115935e`.
Worker continuation: `a7a920ebf298c03851b1c78a4af974e8c91337a7`,
[copied jobs, bounded channel, original SQL and UI-model retirement](software-worker-report.md).
Native selected-app view: `33b9a45cd51c679d743dc4c4a8ae2e4afb5abff8`,
[list/filter/full details, keyboard, controlled IME and independent AT-SPI](software-view-report.md).
Executed original source: Aetheri `46e2a21ef979d9a76db2a8487d1de167a6a79ef7`.
Read-only interface survey: committed Aetheri
`497822f9609e50483594882dd74e5533257a9722`; [survey receipt](software-api-survey.json).
The active Aetheri checkout has independent edits and is not compiled, reset,
replaced or repinned by this work. The surveyed projection header matches the
exercised older header exactly; this is not current-runtime acceptance.

| Required composition | Existing source/evidence | Next consumer work | External owner obligation |
| --- | --- | --- | --- |
| Settings and devices | Original Settings model contains transient display observations and appearance/language/text settings; [example21](../../examples/21_aetheri_model/README.md) and ordinary Settings evidence already exist | Reuse controls/status/wakeup for additional actual device models, with unavailable/pending/error/reconnect states | Real audio, network, Bluetooth/input, power/account observations and authorized control; compositor output observation alone changes no display/device configuration |
| Software/apps | Original `ae_catalogue_resolve_app` returns an exact recipient-scoped active-app observation; [example34](../../examples/34_aetheri_software/README.md) has a bounded worker and native list/filter/full details with keyboard/controlled IME/OS AT-SPI | Native held-query close/wake, foreign clipboard/late-completion, extended navigation and resize/output migration checks; exact artifact/dependency joins | Actual eligible target discovery, full inventory protocol, authenticated source/recipient admission and operation APIs |
| Components/dependencies | Original manifest decoder exposes declared component/version/publisher/files/actions/dependency artifact IDs; [example31](../../examples/31_aetheri_manifest/README.md) already reads verified canonical artifacts | Join a caller-selected manifest only after exact observed artifact/component/publisher agreement; show missing/unverified dependency metadata explicitly | Installed dependency/ownership state, trusted expected artifact provenance and complete inventory; a manifest path list is no installed-files or current-process inventory |
| Processes/resources/reasons | Original execution/unit getters and recovery observations retain exact historical profiles/leases/custody; [example33](../../examples/33_aetheri_recovery/README.md) provides bounded read-only inspection | Present original association/reason data without calling histories running/current; consume an actual process/resource snapshot when supplied | Current original process/FD/unit/image evidence and metrics, source/actor identity and authenticated transport; no adoption from a matching PID/name/SQL row |
| Install/update/remove/full removal | [Operation observer](../../examples/27_aetheri_operation/README.md) uses original journal APIs; [removal review](../../examples/28_removal_review/README.md) distinguishes retained/shared/unknown scope | Compose real owner plans/results with selection/conflict/unknown/reconnect handling; preserve exact target and request identity | Actual plan admission and effects, complete ownership enumeration, shared/retained-state evidence and independent completion; a UI acknowledgement is no deletion/authorization receipt |
| Login/lock/permission | Existing credential controls and semantics can render supplied values; trusted presentation is a separate shell/host seam | Compose approved status/scopes, secret handling and failure/lifetime states with an actual trusted host adapter | Session/caller authorization, secure placement/input routing and lock ordering; ordinary app role/identity strings cannot create trust |
| Installer/first run/recovery | Software rendering, Settings and original recovery metadata exist | Minimal offline presentation host, exact target review, independently observed progress/failure and non-graphical fallback handoff | Storage/encryption/update/recovery admission, operation identity and a usable fallback; restarting a view must not repeat disk/service effects |

The frozen `management-v1.xml` discussion draft explicitly describes interfaces,
not installed services or bus policy. It already declares `ListComponents`,
`ListInstalls`, `GetInstall`, plan/effect and operation methods. Those declarations
must remain part of the intended full management scope. They are not evidence
that an implemented C enumeration adapter or a running service supplied a complete
inventory in the tests here. The concrete selected-install getter is the usable
original source exercised in this increment. No private SQL enumeration is added
to the GUI/SDK to stand in for the missing service integration.

`ResolveAppInstall` itself exposes only eligible active **apps**, not every tool,
library, inactive install or all known components. Its `-ENOENT` intentionally
does not distinguish absent, ineligible and inactive targets. The new model
preserves that ambiguity, complete opaque IDs and revisions. Its before/after
checks reject observed context drift, but grant no transaction/admission across
separate reads. [Model evidence](software-model-report.md) remains narrower than
the final application matrix.

The next composition joins the selected observation to exact verified
artifact/dependency details. Native software-specific lifecycle/clipboard/resize
checks and full inventory/device/trusted service adapters
and the remaining packet 6–8 acceptance stay open. Existing SDK controls and
renderer/input/wakeup contracts should be reused; add a primitive only when the
composed workload demonstrates a missing capability. All 103 catalogue IDs and
the full required surface list remain intact.
