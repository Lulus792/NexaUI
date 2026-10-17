# Completed probe-build cleanup — 2026-10-17

The three completed Windows negative-probe build trees were inspected for
remaining use. No matching build/test process was active and project sources
contained no dependency on their generated build files. Probe source copies,
raw failure logs and committed validation receipts remain available.

The initial batched recursive-removal command was rejected by automatic approval
review. Cleanup instead used each build system's ordinary target:

```powershell
cmake --build D:/NexaUI/out/text-pointer-before/build --config Debug --target clean
cmake --build D:/NexaUI/out/text-command-before/build --config Debug --target clean
cmake --build D:/NexaUI/out/text-command-guard-probes/build --config Debug --target clean
```

All three exited successfully. Logical file sizes fell from 41,181,478 to
1,113,757 bytes: **40,067,721 bytes (about 38.21 MiB) of generated outputs removed**.
This is the sum of file lengths, not an isolated measurement of physical disk
free space. Generated project/configuration files remain for reproducibility.

The current static/shared/sanitizer build trees, debug tools needed for ongoing
work, fonts/dependencies, experiment sources, protected comparison images and
documentation images were retained. The proposed raw-image deletions were not
performed; historical comparison material remains intact.

No VM/WSL disk was touched. New local Linux commands use the existing shared
`Alpine-Dev` distribution. Historical records referring to `NexaUI-Alpine-3.24`
remain unchanged, as required by the local migration instructions.

The completed `out/gsub-context-negative/build` was subsequently checked for
matching active build/probe processes (none) and cleaned through its own CMake
target. Its resolved path was verified inside `D:/NexaUI/out/gsub-context-negative/`.
Logical file sizes fell from 5,069,692 to 321,763 bytes, removing another
4,747,929 bytes. Combined removal is **44,815,650 bytes (about 42.74 MiB)**.
Old GSUB source copies, CMake input/configuration and actual failure logs remain.
Current Design-A images and the unresolved D3D12 diagnostic are retained.
