# Compact numeric field drafts

SDK source `b53513b390dbf135050a858fbdfd434edc04ad08` compacts the generated
numeric field draft when the existing parser verifies the same Double,
including signed zero. Creation and programmatic replacement share this
formatter. It keeps the usual fixed or exponent notation and converts the
formatter's decimal separator to the ASCII point expected by the editor.
The fallback uses full precision. User-entered parsing and other numeric
controls retain their existing implementation.

The exact new test fails against SDK
`8e06b88366be0e064279db9779f571d22611046a` because the initial spelling has
excess digits. The corrected cases exercise positive and negative 0.45,
a high-precision integer, signed zero and an ordinary six-digit integer.
They check the committed value as well as the draft. All 36 selected SDK
CTests and 15 installed Physim CTests pass across static Release, shared
Release and ASan/UBSan/LSan. Existing native v1/v3 routing tests are included.

This Alpine host did not expose a real comma numeric locale. Two separately
linked formatter fixtures supply a comma and a multi-byte separator. Both
execute the unmodified numeric tests and record 20 decimal separator uses.
These results cover normalization under the fixture. Desktop locale and
input-method acceptance remain separate checks.

The unchanged Physim export consumer
`a14cfda4d8c85fa6bb07cbbce773c5412d7192fe` produces identical full RGB
output in the three profiles. The reviewed 802×502 image shows `0.45` in
the angle field. The previously occupied area to its right contains only
the field background. The saved physics snapshot and plot range are
unchanged by the export.

Local image `physim-numeric356.png`. Archive
`nexaui-physim-numeric356.tar.gz` contains 16,729,234 bytes with SHA-256
`0958b55301d5b245053a96efcf3f58d3aeb4a75544adad401e3710f1a51db5d5`.
The [evidence manifest](numeric-draft-format-evidence.json) binds the tests,
old-SDK failure, fixture inputs and images. The
[cross-platform CI](https://github.com/Lulus792/NexaUI/actions/runs/43974152462)
tracks the exact implementation.

This advances T05, D02 and R12. Full locale-specific input and precision
policy, existing parser edge cases and production export design remain open.
