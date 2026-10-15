# NexaUI User Guide

NexaUI is under development. The current usable feature set is described in [Getting started](getting_started.md) and [Core API](api/core.md). [ROADMAP.md](../ROADMAP.md) tracks the remaining native, text, widget, accessibility and simulation work required by the project brief.

System contracts and implementation status: [Layout](concepts/layout.md), [Input](concepts/input.md), [Identity and focus](concepts/focus.md), [Accessibility semantics](concepts/accessibility.md), [Text](concepts/text.md), [Threading](concepts/threading.md), and [Rendering](concepts/rendering.md).

Native rendering: [D3D12](concepts/d3d12.md) and [Linux Vulkan shapes, clips and alpha](concepts/vulkan.md). Vulkan now includes RGBA8/A8 uploads; native macOS remains pending. The [NexaCloud example](../examples/nexacloud/README.md) has Windows/D3D12 and Linux/Vulkan native runners.

The guide will expand with each public API slice. Pages for unimplemented capabilities are deliberately absent to avoid implying support that has not been built or validated.

The initial [AppKit window adapter](concepts/macos.md) has passing native CI
callback/lifecycle checks. Full macOS integration, physical acceptance and the
macOS NexaCloud runner remain pending.

The initial [Metal backend](concepts/metal.md) implements clear/present, shapes,
clips and straight alpha. Opaque GPU comparisons have executed on Apple's virtual
device; full lifecycle/alpha validation and image commands remain pending.
