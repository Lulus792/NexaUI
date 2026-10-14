# 00: Hello window

On Windows, run `nexaui_00_hello_window`. It opens a native window and prints
its logical client size and backing scale. Click or type in the window to see
normalized pointer, key and Unicode text events in the console. Close the
window to exit.

`nexaui_00_hello_window --smoke` creates the window hidden, polls three times,
checks its client geometry and scale, then exits. CTest runs this mode in
static and shared builds. This first example only handles a native window;
the next example builds and displays UI commands.

Native window support is currently implemented on Windows. Other platform
backends are still planned.
