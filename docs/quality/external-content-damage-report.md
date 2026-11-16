# Changed rectangles for external CPU frames

Consumer `c7f3d4bdff8fa6c983e214bd4cabe50fd8ee81ea` uses the installed
Wayland-only SDK `b881dd2c0156bdfe1196f743adf4d9177b647985`. It bounds
producer-to-staging work for embedded CPU content. The producer can describe a
rectangle of changed source pixels and the sequence it used as a base. The
consumer applies the rectangle only when the staged sequence matches within
the same generation and resize request. The payload still contains the entire
current image so the consumer can recover when publication overtakes staging.
Invalid damage is rejected before copying. The renderer retains the existing
viewport lease through consumption.

All 40 installed consumer CTests pass across static Release, shared Release,
ASan/UBSan/LSan and the default native-fixture-off configuration. The new
case uses a 16×12 source with padded rows, compares full rendered output and
checks 24 bytes for a matching 3×2 patch, 768 bytes for a skipped base and no
extra allocation. Publishing two replacements while a renderer lease is
active preserves the prior leased output. The malformed-input case also
exercises invalid base and rectangle metadata. Three compiled counterexamples
are rejected by the same test when forcing the full copy, ignoring the base
or dropping the horizontal offset.

All 12 private Weston/AT-SPI workflows pass in the three native profiles.
Six native runs exercise the patch after an unsupported frame. Each copies
23,296 bytes of the 372,736-byte packed 448×208 image. The independent peer
compares every pixel of the visible image, including the unchanged pixels
outside the 112×52 patch. The archived captures independently match the exact
RGB expectation. The workflow also exercises full-frame staging at the next
resize and existing keyboard, focus, replacement and shutdown checks.

These byte counts cover the staging copy. The renderer still presents complete
frames. The producer emits full payloads, and GPU upload, dirty presentation,
engine integration and native performance timing remain open for R06 and R09.
The example's existing public SDK dependencies remain sufficient.

The [evidence manifest](external-content-damage-evidence.json) binds the
build commands, artifacts and test results. Local archive
`nexaui-content-damage334.tar.gz` has 293,434 bytes and SHA-256
`ea9e2d9696f9f0b0c4913476281bb8fa18d613102d89e9096d577a4980426a69`.
