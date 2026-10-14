# Contributing

Read [MASTER.md](MASTER.md) and [ARCHITECTURE.md](ARCHITECTURE.md) before adding modules. Core runtime functionality must use NexaUI code, the C library, platform SDKs or graphics driver APIs; do not add third-party UI, windowing, rendering, layout, widget, text or font dependencies. Keep native headers inside their respective backend directories.

The configure-time [dependency audit](docs/concepts/dependencies.md) rejects unapproved core link libraries and known forbidden headers. If a platform adapter needs another OS or GPU library, document the use and update the explicit allowlist alongside the adapter.

For each change, document public ownership, lifetime, failure and thread rules; add a focused test for behavior that can fail; run CMake build and CTest with finite timeouts; update documentation and examples when a public call changes. Native backend changes need verification on their actual platform. Report untested platforms explicitly.

Before v1.0, API-breaking changes are allowed with a changelog entry and an architectural reason. Keep commits scoped to a ROADMAP task or a smaller dependency-aware slice.
