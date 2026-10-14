# Sanitizer jobs and local evidence

The `C17 build and test` workflow runs the Linux Clang ASan/UBSan test suite
on pushes and pull requests. Its manual dispatch adds two optional inputs:
`windows_asan` runs MSVC ASan with static and shared libraries;
`fuzz_campaigns` runs the five bounded Clang fuzz targets. All jobs retain
test failures, use read-only repository permissions and have bounded timeouts.
The optional jobs save test logs or fuzz corpora, crash artifacts and reports.
Workflow configuration is not evidence of a hosted run.

## Recorded validation — 2026-10-14

Windows 11 Pro 10.0.26200, x64, MSVC 19.38.33145, Windows SDK 10.0.26100.0,
RelWithDebInfo, `/O2 /Ob1 /DNDEBUG /Zi /fsanitize=address`:

| Library | Test result | Wall time |
| --- | --- | ---: |
| Static | 81 passed, 1 skipped, 0 failed | 34.14 s |
| Shared | 81 passed, 1 skipped, 0 failed | 50.83 s |

The skipped test was the native clipboard test: its isolated window station
was unavailable and the interactive clipboard was occupied. The suites
include core Unicode/text/widget tests, Win32 input and IME, D3D12 readback,
device-loss tests and examples. No ASan finding occurred. The Windows jobs
provide ASan evidence; UBSan and leak auditing are outside their scope.

Alpine 3.24.2 under WSL, Clang 22.1.3, ASan/UBSan, 10,000 runs per target,
seed 1800, fresh copied seed corpora:

| Target | Exit | Final instrumented coverage counters |
| --- | ---: | ---: |
| SFNT | 0 | 731 |
| GSUB | 0 | 640 |
| GPOS | 0 | 629 |
| UTF-8/editing | 0 | 953 |
| Text layout | 0 | 3,194 |

Counters are individual campaign observations, not coverage percentages.
LeakSanitizer was disabled only for the recorded Alpine libFuzzer runtime
issue described in [the text campaign report](text-fuzz-report.md).
The Ubuntu workflow uses the default ASan leak behavior. UBSan uses
`halt_on_error=1`, so a finding cannot become a successful campaign exit.

An independent deliberate 8-byte heap overflow was compiled and detected
under both MSVC ASan and Alpine Clang ASan. The checker accepted the actual
`heap-buffer-overflow` reports and rejected an unrelated process failure.
The campaign runner's five tests passed on both Windows and Alpine; they
cover source-corpus isolation, failure reporting, timeout, success and output
directory reuse. Actionlint 1.7.12 validated the workflow with exit code 0.
Local logs are retained under `out/quality/q07-2026-10-14`.

## Reproduction

Windows PowerShell:

```powershell
cmake -S . -B build-msvc-asan-static -A x64 -DBUILD_SHARED_LIBS=OFF `
  "-DCMAKE_C_FLAGS_RELWITHDEBINFO=/O2 /Ob1 /DNDEBUG /Zi /fsanitize=address" `
  "-DCMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO=/DEBUG /INCREMENTAL:NO" `
  "-DCMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO=/DEBUG /INCREMENTAL:NO"
cmake --build build-msvc-asan-static --config RelWithDebInfo --parallel 4
# Use a developer shell for the selected compiler's runtime DLL on PATH.
ctest --test-dir build-msvc-asan-static -C RelWithDebInfo `
  --output-on-failure --timeout 60 --parallel 4
```

Use a fresh directory and `BUILD_SHARED_LIBS=ON` for the shared variant.
RelWithDebInfo avoids `/RTC`; the linker flags disable incremental linking.
Both are incompatible with MSVC ASan. The workflow finds the selected
compiler from CMake's compiler information and checks its ASan DLL before
adding that directory to PATH. See Microsoft's
[ASan limitations](https://learn.microsoft.com/en-us/cpp/sanitizers/asan-known-issues?view=msvc-170)
and [runtime documentation](https://learn.microsoft.com/en-us/cpp/sanitizers/asan-runtime?view=msvc-170).

After building the Clang fuzz targets as described in [fuzz/README.md](../../fuzz/README.md):

```sh
python3 tools/run_fuzz_campaign.py --build-dir build-fuzz \
  --output-dir out/fuzz-run-1 --runs 10000 --seed 1800
python3 tests/test_fuzz_runner.py
```

The output directory must be new and outside the source corpus. The runner
stores copied seeds, discoveries, logs, executable/seed SHA-256 hashes,
commands, environment, exit codes and elapsed time in `summary.json`.
`--target layout` runs one target; `--wall-timeout` bounds each subprocess.
On the recorded Alpine runtime only, prefix the campaign command with
`ASAN_OPTIONS=detect_leaks=0`.

The standalone counterexample is built from `tests/sanitizer_probe` and
verified with `python tests/check_sanitizer_probe.py <asan_probe>`.
It is deliberately excluded from ordinary builds and normal application
execution.

## Remaining evidence

The new jobs have not run on GitHub-hosted runners. macOS sanitizers,
ThreadSanitizer, actual monitor/input transitions and the skipped Windows
clipboard case remain unverified by these runs. Bounded fuzz campaigns do
not prove memory safety for all inputs or validate rendering quality.
