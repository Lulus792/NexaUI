# Accessibility tree and keyboard example

Run `nexaui_accessibility_example`. It creates a headless UI context, moves
focus with Tab, checks a box with Space, wraps past a disabled button, and
activates another button with Enter. The program verifies the resulting
parent-first semantic snapshot and prints its roles, names, value and states.
No font, native window or output file is needed.

The root has two active controls, one disabled button and a status node.
Their IDs remain stable across frames. The example checks accessible names,
parent IDs, depth, role, action flags, checked state, disabled state and
derived focus state. The status value changes to `Details opened` after the
keyboard action. Any failed check exits nonzero; CTest runs the example.

This exercises the backend-independent semantic model. NexaUI does not yet
expose this tree through Windows UI Automation, macOS accessibility or Linux
AT-SPI, so the output is a developer inspection aid and cannot be used as a
screenreader acceptance result.
