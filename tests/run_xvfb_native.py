"""Run a native X11 test on an isolated Xvfb display."""

import os
import select
import secrets
import subprocess
import sys
import tempfile


def main() -> int:
    if len(sys.argv) < 2:
        print("usage: run_xvfb_native.py COMMAND [ARGS...]", file=sys.stderr)
        return 2
    with tempfile.TemporaryDirectory(prefix="nexaui-xvfb-") as temporary:
        authority = os.path.join(temporary, "Xauthority")
        cookie = secrets.token_hex(16)
        xauth = os.environ.get("NEXAUI_XAUTH", "xauth")
        subprocess.run([xauth, "-f", authority, "add", ":0",
                        "MIT-MAGIC-COOKIE-1", cookie], check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        server = subprocess.Popen(
            [os.environ.get("NEXAUI_XVFB", "Xvfb"), "-displayfd", "1",
             "-screen", "0", "1024x768x24", "-auth", authority,
             "-nolisten", "unix", "-listen", "tcp"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        try:
            assert server.stdout is not None
            if not select.select([server.stdout], [], [], 10)[0]:
                print("Xvfb startup timed out", file=sys.stderr)
                return 1
            display_number = server.stdout.readline().strip()
            if not display_number.isdecimal():
                print("Xvfb did not provide a display number", file=sys.stderr)
                return 1
            subprocess.run([xauth, "-f", authority, "add",
                            f"localhost:{display_number}",
                            "MIT-MAGIC-COOKIE-1", cookie], check=True,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            env = os.environ.copy()
            env["DISPLAY"] = f"localhost:{display_number}"
            env["XAUTHORITY"] = authority
            env["NEXAUI_PLATFORM"] = "x11"
            env["NEXAUI_TEST_PRIVATE_X11"] = "1"
            return subprocess.run(sys.argv[1:], env=env, timeout=60,
                                  check=False).returncode
        finally:
            server.terminate()
            try:
                server.wait(timeout=5)
            except subprocess.TimeoutExpired:
                server.kill()
                server.wait()


if __name__ == "__main__":
    raise SystemExit(main())
