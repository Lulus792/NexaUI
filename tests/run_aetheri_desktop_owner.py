"""Exercise the original Aetheri supervisor with a NexaUI shell test host."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import shutil
import socket
import subprocess
import sys
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("fixture", "owner", "server", "desktop", "settings", "work-dir"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--terminal", type=Path)
    parser.add_argument("--shell", type=Path)
    args = parser.parse_args()
    if bool(args.terminal) != bool(args.shell):
        parser.error("--terminal and --shell must be supplied together")
    fixture, owner, server, desktop, settings = (
        p.resolve(strict=True) for p in (args.fixture, args.owner, args.server, args.desktop, args.settings))
    root = args.work_dir.resolve()
    root.mkdir(mode=0o700)
    build = root / "build"
    build.mkdir(mode=0o700)
    for name, target in (("test_compositor_server", server), ("aetheri-desktop", desktop),
                         ("aetheri-settings", settings)):
        (build / name).symlink_to(target)
    if args.terminal:
        terminal = args.terminal.resolve(strict=True)
        shell = args.shell.resolve(strict=True)
        (build / "aetheri-terminal").symlink_to(terminal)
        # Terminal opens the shell with O_NOFOLLOW; keep an ordinary copied file.
        shutil.copy2(shell, build / "aetheri-shell")
    for name in ("captures", "sessions", "sessions/fixture", "sessions/fixture/runtime", "sessions/fixture/cache"):
        (root / name).mkdir(mode=0o700)
    controller, issuer = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    controller.settimeout(5)
    def digest(path):
        with path.open("rb") as stream:
            return hashlib.file_digest(stream, "sha256").hexdigest()
    result = {"status": "failed", "scope": "original supervisor, isolated headless compositor and NexaUI test host",
              "fixture_sha256": digest(fixture), "owner_sha256": digest(owner),
              "server_sha256": digest(server), "desktop_sha256": digest(desktop),
              "settings_sha256": digest(settings)}
    result["terminal_case"] = "not-run"
    if args.terminal:
        result.update(terminal_sha256=digest(terminal), shell_sha256=digest(shell))
    child = None
    try:
        with (root / "owner.log").open("wb") as stream:
            child = subprocess.Popen([sys.executable, str(fixture), str(build), str(root),
                                      str(issuer.fileno()), str(owner)], cwd=root,
                                     pass_fds=(issuer.fileno(),), stdin=subprocess.DEVNULL,
                                     stdout=stream, stderr=subprocess.STDOUT)
            issuer.close()
            def log():
                return (root / "owner.log").read_text(errors="replace")
            def wait(predicate, label):
                end = time.monotonic() + 20
                while not predicate():
                    if child.poll() is not None or time.monotonic() > end:
                        raise RuntimeError("waiting for " + label + ": " + log()[-2500:])
                    time.sleep(.02)
            def event(value):
                controller.sendall(value.encode("ascii"))
                if controller.recv(32) != b"OK":
                    raise RuntimeError("input rejected: " + value)
            def tap(code):
                event(f"K {code} 1")
                event(f"K {code} 0")
            def wait_app_focus(app_id, selector):
                marker = "desktop: LAUNCH_REPLY A " + selector + "\n"
                def restored():
                    text = log()
                    if marker not in text:
                        return False
                    after = text.rsplit(marker, 1)[1]
                    return ("desktop: LEAVE\n" in after and
                            bool(re.search(r"^desktop: WINDOW \d+ 1 .* \| " +
                                           re.escape(app_id) + r"$", after, re.M)))
                wait(restored, "search collapse and app focus")
            def launch(name):
                selector = "S" if name == "settings" else "T"
                codes = {"s":31,"e":18,"t":20,"i":23,"n":49,"g":34,
                         "r":19,"m":50,"a":30,"l":38}
                prior = log().count("desktop: TOGGLE 0\n")
                prior_focus = log().count("desktop: ENTER 0\n")
                query_line = "desktop: QUERY 0 " + name + "\n"
                prior_query = log().count(query_line)
                event("M 100 100")
                event("K 125 1")
                tap(57)
                event("K 125 0")
                wait(lambda: log().count("desktop: TOGGLE 0\n") > prior, "search toggle")
                wait(lambda: log().count("desktop: ENTER 0\n") > prior_focus, "search focus")
                for char in name:
                    tap(codes[char])
                wait(lambda: log().count(query_line) > prior_query, "search query")
                request = "desktop: LAUNCH_REQUEST " + selector + "\n"
                prior = log().count(request)
                tap(28)
                wait(lambda: log().count(request) > prior, "launch request")
            wait(lambda: "FIXTURE_NOTIFY READY=1" in log() and "desktop: READY nexaui-layer\n" in log(),
                 "supervisor and desktop")
            launch("settings")
            wait(lambda: len(re.findall(r"^SPAWN settings \d+$", log(), re.M)) == 1 and
                 "settings: MODEL theme=0 german=0 text=100" in log() and
                 "desktop: LAUNCH_REPLY A S\n" in log(), "first owner-launched Settings")
            first = int(re.findall(r"^SPAWN settings (\d+)$", log(), re.M)[0])
            wait(lambda: "desktop: WINDOW " in log() and "org.aetheri.Settings" in log(), "Settings window observation")
            wait_app_focus("org.aetheri.Settings", "S")
            replies = log().count("desktop: LAUNCH_REPLY A S\n")
            launch("settings")
            wait(lambda: log().count("desktop: LAUNCH_REPLY A S\n") > replies, "existing process acknowledgement")
            wait_app_focus("org.aetheri.Settings", "S")
            if len(re.findall(r"^SPAWN settings \d+$", log(), re.M)) != 1:
                raise RuntimeError("owner created a duplicate Settings process")
            event("K 29 1")
            tap(16)
            event("K 29 0")
            wait(lambda: "desktop: LAUNCH_REPLY X S\n" in log(), "Settings exit and pipe drain")
            launch("settings")
            def reopened():
                text = log()
                spawns = list(re.finditer(r"^SPAWN settings \d+$", text, re.M))
                return len(spawns) == 2 and "settings: MODEL theme=0 german=0 text=100" in text[spawns[-1].end():]
            wait(reopened, "reopened Settings")
            second = int(re.findall(r"^SPAWN settings (\d+)$", log(), re.M)[1])
            if first == second:
                raise RuntimeError("owner did not create a new process after drain")
            wait(lambda: bool(re.search(r"^desktop: WINDOW \d+ \d+ .* \| org\.aetheri\.Settings$",
                                        log().split("SPAWN settings " + str(second), 1)[1], re.M)),
                 "reopened Settings window observation")
            wait_app_focus("org.aetheri.Settings", "S")
            if args.terminal:
                launch("terminal")
                wait(lambda: len(re.findall(r"^SPAWN terminal \d+$", log(), re.M)) == 1 and
                     "terminal: READY wayland opengles2 shell=" in log() and
                     "desktop: LAUNCH_REPLY A T\n" in log() and
                     bool(re.search(r"^desktop: WINDOW \d+ \d+ .* \| org\.aetheri\.Terminal$", log(), re.M)),
                     "owner-launched Terminal")
                wait_app_focus("org.aetheri.Terminal", "T")
                replies = log().count("desktop: LAUNCH_REPLY A T\n")
                launch("terminal")
                wait(lambda: log().count("desktop: LAUNCH_REPLY A T\n") > replies,
                     "existing Terminal acknowledgement")
                wait_app_focus("org.aetheri.Terminal", "T")
                if len(re.findall(r"^SPAWN terminal \d+$", log(), re.M)) != 1:
                    raise RuntimeError("owner created a duplicate Terminal process")
                result["terminal_case"] = "passed"
            child.send_signal(signal.SIGTERM)
            if child.wait(timeout=15):
                raise RuntimeError("supervisor shutdown failed")
            ending = re.search(r"^SESSION_END (.*)$", log(), re.M)
            if not ending:
                raise RuntimeError("missing owner exit report")
            if "settings: SHUTDOWN signal\n" not in log().split("SPAWN settings " + str(second), 1)[1]:
                raise RuntimeError("reopened Settings did not report signal shutdown")
            summary = json.loads(ending[1])
            expected_children = {"compositor": 0, "desktop": 0, "settings": 0}
            if args.terminal:
                expected_children["terminal"] = 0
                terminal_end = re.search(r"^terminal: CLOSED .*failed=0$", log(), re.M)
                settings_end = log().rfind("settings: SHUTDOWN signal\n")
                if not terminal_end or terminal_end.start() >= settings_end:
                    raise RuntimeError("terminal did not drain before Settings shutdown")
            if summary["children"] != expected_children:
                raise RuntimeError("unexpected child shutdown: " + repr(summary))
            if not any(item["role"] == "settings" and item["pid"] == first and item["status"] == 0
                       for item in summary["app_history"]):
                raise RuntimeError("first Settings was not reaped and drained")
            result.update(status="passed", first_pid=first, reopened_pid=second)
    finally:
        if child and child.poll() is None:
            child.terminate()
            try:
                child.wait(timeout=15)
            except subprocess.TimeoutExpired:
                # Keep the original supervisor alive rather than orphaning
                # children whose process/pipe drain it still owns.
                result["cleanup"] = "supervisor-still-live"
                result["owner_pid"] = child.pid
        issuer.close()
        controller.close()
        result["owner_exit"] = child.returncode if child else None
        (root / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
