# Keyboard and text event storage lifetime

SDK `9056af80fb2ccf2be0e2839e9cef76ba46cc8660` is a development snapshot of
0.1.33. The platform event queue and UI context now overwrite retired
keyboard and text-system records. The native queue retires a record when a
caller takes it or routing succeeds. A failed route retains the undelivered
event for retry. The context retires a frame's records at the next successful
`nx_begin_frame`. Growing and destroying either queue also overwrite the
input records in memory that is about to be released. [Input contract](../concepts/input.md)

The regression observes backing memory before deallocation through the public
allocator contract and verifies transfer, wraparound, growth, partial routing
failure, expiry and teardown. It checks that a failed `nx_begin_frame` leaves
the current array readable. The same test source fails against platform source
`6c5cd56` and installed SDK `529d5a9` at native dequeue and context growth.
The updated shared package has the same 546 exported symbol names as that
reference.

Clang 22.1.3 on Alpine-Dev ran **51 SDK CTests** and **3 installed login CTests**
across static Release, shared Release and Debug `-O1` ASan/UBSan/LSan. The SDK
cases include Wayland v1/v3 wire tests and the protected-field atomic IME case.
Three additional private Weston login workflows pass with the original
`6eda6feb98b37048ca84d73b65e24f919297987c` Aetheri model and independent AT-SPI
peer. The login workflows exercise keyboard input. [Local receipt](input-event-storage-local.json)

The initial validation environment still inherited a WSLg runtime directory
with the default `wayland-0` socket. The no-display assertion failed in one
CTest per profile. Repeating with an empty private runtime directory passed
using the same binaries. Both rounds retain the original CTest deadlines and
complete logs. Static and shared builds use C17 with warnings as errors.

`nexaui-event-storage174.tar.gz` contains 30,744,236 bytes. The SHA-256 is
`b911024220060e2d355229f8e27f25038d08093475b50dde2d9139449780dcf9`.
All 101 members were read back and verified. The platform CI is tracked
separately. [Main CI run](https://github.com/Lulus792/NexaUI/actions/runs/43750160184)

This advances T06/P05/P08/Q02 for the toolkit's two event stores. OS and IME
transport buffers, application copies and native login IME acceptance remain
separate work. Trusted placement and login service admission stay with the
Aetheri owner.
