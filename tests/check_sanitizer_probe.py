"""Accept only a nonzero ASan heap-overflow report from the deliberate probe."""

from pathlib import Path
import subprocess
import sys


def main():
    if len(sys.argv) != 2:
        print("usage: check_sanitizer_probe.py <asan_probe>", file=sys.stderr)
        return 2
    binary = Path(sys.argv[1]).resolve()
    try:
        result = subprocess.run([str(binary), "8"], stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, timeout=30, check=False)
    except (OSError, subprocess.TimeoutExpired) as error:
        print(f"probe could not execute: {error}", file=sys.stderr)
        return 1
    output = result.stdout.decode("utf-8", errors="replace")
    if result.returncode == 0 or "AddressSanitizer" not in output or "heap-buffer-overflow" not in output:
        print(output, file=sys.stderr)
        print(f"expected an ASan heap-buffer-overflow report; exit={result.returncode}", file=sys.stderr)
        return 1
    print("ASan counterexample detected: heap-buffer-overflow")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
