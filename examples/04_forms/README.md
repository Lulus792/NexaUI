# 04: Forms

Run `nexaui_04_forms FONT_FILE OUTPUT_PREFIX` with a local TrueType font.
The headless example writes `OUTPUT_PREFIX-invalid.ppm` and
`OUTPUT_PREFIX-submitted.ppm`.

The form has two validated text fields, a confirmation checkbox, a submit
button, and a status message. The button is disabled until both fields are
valid and the checkbox is checked. The program tries the disabled button,
edits both fields, clicks the checkbox label, then submits with Space. It
checks the field values, semantic invalid/checked/disabled states, and the
submission result while rendering the initial and final states.

This example introduces composition and validation after `03_layout`.
It uses the headless renderer so it runs on every supported build platform.
