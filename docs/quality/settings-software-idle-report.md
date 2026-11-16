# Settings idle with hidden Wayland software presentation

Settings revision `ed5e86de31c8842be31de989ee3fa04ed5a17728` waits for
events after its software view returns busy or suspended. A geometry change,
routed native event or application-worker result can rebuild the pending
frame. A poll timeout can still retry unavailable accessibility registration.
The runner keeps Vulkan suspension separate because transient swapchain
conditions can require a retry without a new native event.

The installed comparison uses SDK
`b881dd2c0156bdfe1196f743adf4d9177b647985`, private Weston 14.0.2/Xvfb on
Alpine WSL2, Clang 22.1.3 Release and an Intel Xeon W-2123. The baseline is
`a65a140439b8e9e0d4be06b5833e1a58a43dff05` with only a hidden-window flag and
counters added for the probe. Each program runs three sequential visible and
three sequential hidden samples of approximately three seconds with a
deliberately unavailable AT-SPI bus. The hidden surface uses
`NX_WINDOW_HIDDEN` and the software view reports suspension on each attempt.

| Release path | Poll calls | Render attempts | Submissions | Process CPU |
| --- | ---: | ---: | ---: | ---: |
| Hidden baseline | 174 / 174 / 171 | 174 / 173 / 170 | 0 / 0 / 0 | 217 / 226 / 266 ms |
| Hidden candidate | 4 / 4 / 4 | 1 / 1 / 1 | 0 / 0 / 0 | 4.4 / 6.9 / 4.6 ms |
| Visible candidate | 5 / 5 / 5 | 1 / 1 / 1 | 1 / 1 / 1 | 8.6 / 8.6 / 10.9 ms |

The same new native guard fails on the baseline because it retries the hidden
surface over 170 times. All 21 installed native probes pass across static
Release, shared Release and ASan/UBSan/LSan. With available AT-SPI, each
profile also records a long event-driven wait on a visible window. The root
build passes seven selected Settings tests, including the newly registered
hidden-window case, renderer selection, visible/unavailable and
visible/available idle, keyboard editing and keyboard controls.
[Measurements and test results](settings-software-idle-evidence.json)

The samples measure app process CPU, poll calls, renderer attempts and
successful submissions. They do not measure OS scheduler wakeups, GPU cost or
display latency. A hidden window created by the app has a reliable existing
software outcome. Observable compositor minimization and resume, Vulkan idle
and the original Aetheri production Settings path remain open for P01.
The [GitHub CI](https://github.com/Lulus792/NexaUI/actions/runs/43968268192)
tracks the exact Settings source.
