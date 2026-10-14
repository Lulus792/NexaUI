# 03 Interactive region

Run `nexaui_interactive_region [prefix]`. It writes `<prefix>_0.ppm`, `<prefix>_1.ppm`, and `<prefix>_2.ppm` for idle, held, and clicked states. The program queues synthetic pointer events to demonstrate stable IDs, last-frame hit testing and frame interaction state. It also registers a semantic window/button tree and prints the accessible name, focus state and actions. This is a custom-widget foundation; visible text, native screenreader bridges and GPU display remain planned.
