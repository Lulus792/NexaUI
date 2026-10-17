# README structure review — 2026-10-17

The current primary READMEs of [Dear ImGui](https://github.com/ocornut/imgui/blob/master/docs/README.md),
[Flutter](https://github.com/flutter/flutter/blob/master/README.md) and
[Windows Terminal](https://github.com/microsoft/terminal/blob/main/README.md)
were read as documentation references. No product code, wording or artwork was
copied into NexaUI.

The useful patterns are a concrete opening description and small usage example
(ImGui), short routes into installation/documentation/contribution (Flutter),
and explicit requirements plus separate build/run guidance (Terminal).
NexaUI uses those structural ideas with its actual C17 API, CMake consumer
instructions, working examples, platform matrix and candid early-development
limits. The new paint/area APIs link to their contracts and validation rather
than presenting every planned feature as complete. A target platform in a table
is distinct from physical-device acceptance.
