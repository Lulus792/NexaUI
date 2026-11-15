# Aetheri login prompt reading

Consumer `558f5755cf113fb9cae3d78cb0eb6a81bca0c8e6` builds the conversation
from Aetheri `6eda6feb98b37048ca84d73b65e24f919297987c` against NexaUI 0.1.33
from `529d5a9d102d975f4fa0664df4936d343bc63b58`.

One wrapped layout provides the height and the visible text. Overflow opens the
existing vertical scroll area and starts focus on its scrollbar. The text is
clipped in the reading area, and its semantic label carries the complete prompt.
The answer and cancellation controls stay below the reading area. A resize that
removes the scrollbar restores focus to the response or first action. A later
overflow starts at the top.

The fixture sends two synthetic 4,096-byte prompts through the original parser.
At 240 × 200, both top frames match pixel for pixel although the final glyph
differs. The difference becomes visible only inside the reading area after End.
Page Down advances the thumb, Page Up and Home return to the original frame,
and Tab reaches the field, Answer and Cancel. The test checks the complete
semantic label, the resize focus transfer and an explicit cancellation. The
unchanged final test against view `9aa8796` fails at the missing scroll area.

All three installed profiles passed the headless workflow under the existing
15-second deadline on Alpine-Dev/Clang 22.1.3. The profiles were static Release,
shared Release and Debug `-O1` ASan/UBSan/LSan. The first implementation omitted
the clip around short labels. The fixture font has no pixels for the unavailable
message, and its render check rejected the resulting empty command stream.
The final implementation clips short labels too. The failure log and old-view
counterexample are retained. [Local receipt](aetheri-login-scroll-local.json)

The raw archive `nexaui-login-view164.tar.gz` contains 4,331,010 bytes. SHA-256
is `debf0ace9c373954aa625e48e7e99d32cabbfdb0cdc85417fc90e3b354ad8213`.
All 49 members were read back and verified.

This extends packet 6 for D06/I01/A01/A06/L09. Native screen-reader access,
IME, compositor and credential-event lifetime checks, trusted placement and
login service admission remain open. [Original composition](aetheri-login-view-report.md)
