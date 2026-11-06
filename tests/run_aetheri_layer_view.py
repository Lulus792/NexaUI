"""Exercise the NexaUI model view on Aetheri's isolated test shell connection."""
import argparse
import array
import gzip
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import shutil
import socket
import subprocess
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("server", "desktop", "font", "work-dir"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--test-held-removal", action="store_true",
                        help="expose the currently unresolved pressed-device removal case")
    parser.add_argument("--app-client", type=Path,
                        help="independent Aetheri compositor test client")
    parser.add_argument("--test-stale-activation", action="store_true",
                        help="close the clicked window before the test owner validates it")
    parser.add_argument("--test-stale-after-collapse", action="store_true",
                        help="close the clicked window after search collapses")
    parser.add_argument("--test-launch-channel", action="store_true",
                        help="exercise the fixed three-byte launch protocol with a test owner")
    parser.add_argument("--second-output-scale2", action="store_true",
                        help="expect a controlled test compositor with its second virtual output at 2x")
    parser.add_argument("--dynamic-output-scale", action="store_true",
                        help="use the test-only Z command to change the second output scale")
    parser.add_argument("--test-invalid-launch", choices=("uncorrelated", "oversized", "ancillary", "peer-loss"),
                        help="require the test host to reject an invalid owner response")
    args = parser.parse_args()
    if args.test_invalid_launch and not args.test_launch_channel:
        parser.error("--test-invalid-launch requires --test-launch-channel")
    if args.dynamic_output_scale and not args.second_output_scale2:
        parser.error("--dynamic-output-scale requires --second-output-scale2")
    server, desktop, font = (p.resolve(strict=True) for p in (args.server, args.desktop, args.font))
    app_client = args.app_client.resolve(strict=True) if args.app_client else None
    root = args.work_dir.resolve()
    root.mkdir(mode=0o700)
    runtime, captures = root / "runtime", root / "captures"
    runtime.mkdir(mode=0o700)
    captures.mkdir(mode=0o700)
    env = dict(os.environ, XDG_RUNTIME_DIR=str(runtime), WAYLAND_DISPLAY="nexaui-layer",
               NEXAUI_DESKTOP_FONT=str(font))
    for key in ("DISPLAY", "WAYLAND_SOCKET"):
        env.pop(key, None)
    controller, issuer = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    shell_server, shell_client = socket.socketpair(socket.AF_UNIX, socket.SOCK_STREAM)
    launch_owner, launch_client = (socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
                                   if args.test_launch_channel else (None, None))
    processes, streams, expected_exits = [], [], set()
    def digest(path):
        with path.open("rb") as stream:
            return hashlib.file_digest(stream, "sha256").hexdigest()

    result = {"status": "failed", "scope": "isolated Aetheri test compositor, virtual outputs",
              "server_sha256": digest(server), "desktop_sha256": digest(desktop),
              "font_sha256": digest(font), "window_observation": "not-run",
              "launch_channel": "not-run"}
    if app_client:
        result["app_client_sha256"] = digest(app_client)
    try:
        def start(binary, name, arguments, inherited, environment):
            stream = (root / (name + ".log")).open("wb")
            streams.append(stream)
            child = subprocess.Popen([str(binary), *map(str, arguments)], env=environment,
                                     stdin=subprocess.PIPE, stdout=stream, stderr=subprocess.STDOUT,
                                     pass_fds=inherited)
            processes.append(child)
            return child

        def log(name):
            return (root / (name + ".log")).read_text(errors="replace")

        def wait(predicate, label):
            end = time.monotonic() + 20
            while not predicate():
                if any(p.poll() is not None and p not in expected_exits for p in processes):
                    raise RuntimeError("process exited before " + label)
                if time.monotonic() > end:
                    raise RuntimeError("timeout: " + label)
                time.sleep(.02)

        def command(child, value):
            child.stdin.write(value.encode("ascii"))
            child.stdin.flush()

        def capture(number):
            compositor.send_signal(signal.SIGUSR1)
            wait(lambda: all(f"CAPTURE {number} {i}\n" in log("server") for i in range(3)),
                 "capture")
            raw = (captures / f"capture-{number}-output-0.ppm").read_bytes()
            magic, dimensions, maximum, pixels = raw.split(b"\n", 3)
            if magic != b"P6" or dimensions != b"1920 1080" or maximum != b"255" or len(pixels) != 1920 * 1080 * 3:
                raise RuntimeError("invalid capture")
            result[f"capture_{number}_sha256"] = hashlib.sha256(raw).hexdigest()
            def pixel(x, y):
                index = (y * 1920 + x) * 3
                return tuple(pixels[index:index + 3])
            return pixel

        compositor = start(server, "server", ("--headless", env["WAYLAND_DISPLAY"], captures,
                           issuer.fileno(), "--shell-fd", shell_server.fileno()),
                           (issuer.fileno(), shell_server.fileno()), env)
        issuer.close()
        shell_server.close()
        wait(lambda: "READY " in log("server"), "compositor")
        desktop_env = dict(env, WAYLAND_SOCKET=str(shell_client.fileno()))
        if app_client:
            desktop_env["NEXAUI_TEST_ACTIVATE"] = "1"
        if args.test_stale_activation:
            if not app_client:
                raise RuntimeError("stale activation case requires --app-client")
            desktop_env["NEXAUI_TEST_HOLD_ACTIVATION"] = "1"
        if args.test_stale_after_collapse:
            if not app_client or args.test_stale_activation:
                raise RuntimeError("after-collapse case requires --app-client and excludes --test-stale-activation")
            desktop_env["NEXAUI_TEST_HOLD_AFTER_COLLAPSE"] = "1"
        inherited = (shell_client.fileno(),)
        if launch_client:
            desktop_env["NEXAUI_TEST_LAUNCH_FD"] = str(launch_client.fileno())
            inherited += (launch_client.fileno(),)
            launch_owner.settimeout(5)
        client = start(desktop, "desktop", (), inherited, desktop_env)
        shell_client.close()
        if launch_client:
            launch_client.close()
        logical_widths = (1920, 1280 if args.second_output_scale2 else 2560, 3840)
        wait(lambda: all(f"PAINT {i} {width} 84\n" in log("desktop")
                         for i, width in enumerate(logical_widths)), "three docks")
        if args.second_output_scale2:
            wait(lambda: "BACKING 1 2 2560 168\n" in log("desktop"), "second output 2x buffer")
        if args.test_invalid_launch:
            if args.test_invalid_launch == "uncorrelated":
                launch_owner.sendall(b"A3T")
            elif args.test_invalid_launch == "oversized":
                launch_owner.sendall(b"X3T-too-long")
            elif args.test_invalid_launch == "ancillary":
                fd = os.open("/dev/null", os.O_RDONLY)
                try:
                    launch_owner.sendmsg([b"X3T"], [(socket.SOL_SOCKET, socket.SCM_RIGHTS,
                                                      array.array("i", [fd]))])
                finally:
                    os.close(fd)
            else:
                launch_owner.close()
            if client.wait(timeout=10) == 0:
                raise RuntimeError("invalid owner response did not fail the test host")
            expected = {
                "uncorrelated": "ae_desktop_model_reply(&v->model,target,bytes[0])",
                "oversized": "n==3 && !extra",
                "ancillary": "n==3 && !extra",
                "peer-loss": "fds[2].revents&(POLLERR|POLLHUP|POLLNVAL)",
            }[args.test_invalid_launch]
            if expected not in log("desktop"):
                raise RuntimeError("host failed for a different reason")
            if "LAUNCH_REPLY " in log("desktop"):
                raise RuntimeError("invalid owner response changed model state")
            result["invalid_launch_case"] = args.test_invalid_launch
            result["status"] = "passed"
            return
        dock = capture(1)
        result["dock_background"] = dock(100, 100)
        result["dock_button"] = dock(20, 1015)
        if dock(100, 100) != (16, 20, 28) or dock(20, 1015) != (128, 175, 255):
            raise RuntimeError("dock pixels missing or transparent background incorrect")
        for index, width, height in ((1, 2560, 1440), (2, 3840, 2160)):
            raw = (captures / f"capture-1-output-{index}.ppm").read_bytes()
            magic, dimensions, maximum, pixels = raw.split(b"\n", 3)
            if (magic != b"P6" or dimensions != f"{width} {height}".encode() or
                    maximum != b"255" or len(pixels) != width * height * 3):
                raise RuntimeError("invalid output capture")
            scale = 2 if index == 1 and args.second_output_scale2 else 1
            offset = ((height - 65 * scale) * width + 20 * scale) * 3
            if tuple(pixels[offset:offset + 3]) != (128, 175, 255):
                raise RuntimeError("dock pixels missing on output " + str(index))
            result[f"dock_output_{index}_sha256"] = hashlib.sha256(raw).hexdigest()
        command(client, "e")
        wait(lambda: "PAINT 0 1920 1080\n" in log("desktop"), "expanded search")
        wait(lambda: "ENTER 0\n" in log("desktop"), "search keyboard focus")
        # The configure callback may submit its buffer after the sync requested
        # by 'e'. Submit a focused frame and roundtrip it before capture.
        command(client, "b")
        wait(lambda: "SYNC b\n" in log("desktop"), "focused search buffer processed")
        search = capture(2)
        result["search_field"] = search(20, 20)
        if search(20, 20) != (38, 44, 54):
            raise RuntimeError("search field pixels missing")
        controller.settimeout(5)
        def event(value):
            controller.sendall(value.encode("ascii"))
            if controller.recv(32) != b"OK":
                raise RuntimeError("input rejected")
        def key(code):
            for down in (1, 0):
                event(f"K {code} {down}")
        if args.second_output_scale2:
            # The compositor's pointer coordinates remain logical; the
            # second output begins at x=1920 and is 720 logical pixels high.
            event("M 1940 655")
            event("B 272 1")
            event("B 272 0")
            wait(lambda: "INTENT 1 1 0 0\n" in log("desktop"), "2x logical dock hit")
            result["second_output_logical_hit"] = "passed"
        if args.dynamic_output_scale:
            event("M 100 100")  # Remove hover before comparing the dock color.
            def second_dock_pixel(number, scale):
                raw = (captures / f"capture-{number}-output-1.ppm").read_bytes()
                magic, dimensions, maximum, pixels = raw.split(b"\n", 3)
                if magic != b"P6" or dimensions != b"2560 1440" or maximum != b"255" or len(pixels) != 2560*1440*3:
                    raise RuntimeError("invalid dynamic-scale output capture")
                # The first button may retain hover after the pointer leaves;
                # sample the separate last button at the same logical inset.
                offset = ((1440 - 65 * scale) * 2560 + 2560 - 20 * scale) * 3
                return tuple(pixels[offset:offset+3])
            for number, scale, expected, inconsistent in (
                    (3, 1, "BACKING 1 1 2560 84\n", "BACKING 1 2 5120 168\n"),
                    (4, 2, "BACKING 1 2 2560 168\n", "BACKING 1 1 1280 84\n")):
                before = log("desktop").count(expected)
                before_inconsistent = log("desktop").count(inconsistent)
                event(f"Z 1 {scale}")
                wait(lambda: log("desktop").count(expected) > before, f"second output scale {scale}")
                syncs = log("desktop").count("SYNC b\n")
                command(client, "b")
                wait(lambda: log("desktop").count("SYNC b\n") > syncs, "scaled dock buffer processed")
                if log("desktop").count(inconsistent) != before_inconsistent:
                    raise RuntimeError("presented a buffer with mixed scale and configure state")
                capture(number)
                if second_dock_pixel(number, scale) != (128, 175, 255):
                    raise RuntimeError(f"dock pixels missing after scale {scale}")
            result["dynamic_output_scale"] = "passed"
        for code in (20, 18, 19, 50, 23, 49, 30, 38):
            key(code)
        wait(lambda: "QUERY 0 terminal\n" in log("desktop"), "typed search")
        key(14)
        wait(lambda: "QUERY 0 termina\n" in log("desktop"), "backspace")
        key(38)
        wait(lambda: log("desktop").count("QUERY 0 terminal\n") >= 2, "corrected search")
        key(28)
        wait(lambda: "INTENT 0 2 2 0\n" in log("desktop"), "terminal launch intent")
        if launch_owner:
            if launch_owner.recv(16) != b"L3T":
                raise RuntimeError("wrong terminal launch packet")
            key(28)
            launch_owner.settimeout(.3)
            try:
                duplicate = launch_owner.recv(16)
            except TimeoutError:
                duplicate = None
            finally:
                launch_owner.settimeout(5)
            if duplicate is not None:
                raise RuntimeError("pending terminal request was duplicated")
            paints = log("desktop").count("PAINT 0 1920 1080\n")
            launch_owner.sendall(b"E3T")
            wait(lambda: "LAUNCH_REPLY E T\n" in log("desktop") and
                 log("desktop").count("PAINT 0 1920 1080\n") > paints, "terminal rejection applied")
        event("M 60 90")
        event("B 272 1")
        event("B 272 0")
        wait(lambda: log("desktop").count("INTENT 0 2 2 0\n") == 2, "terminal pointer intent")
        if launch_owner:
            if launch_owner.recv(16) != b"L3T":
                raise RuntimeError("wrong terminal retry packet")
            launch_owner.sendall(b"A3T")
            wait(lambda: "LAUNCH_REPLY A T\n" in log("desktop"), "terminal process acknowledged")
            launch_owner.sendall(b"X3T")
            wait(lambda: "LAUNCH_REPLY X T\n" in log("desktop"), "terminal process reaped")
        event("M 10 200")
        event("B 272 1")
        event("M 60 90")
        event("B 272 0")
        command(client, "p")
        wait(lambda: "SYNC p\n" in log("desktop"), "outside press processed")
        if log("desktop").count("INTENT 0 2 2 0\n") != 2:
            raise RuntimeError("outside press unexpectedly activated a result")
        if args.test_held_removal:
            event("M 60 90")
            event("B 272 1")
        event("D")
        wait(lambda: "POINTER REMOVED\n" in log("desktop") and "KEYBOARD REMOVED\n" in log("desktop"),
             "input devices removed")
        event("A")
        wait(lambda: log("desktop").count("ENTER 0\n") >= 2, "search focus after device replacement")
        event("M 60 90")
        command(client, "p")
        wait(lambda: log("desktop").count("SYNC p\n") >= 2, "replacement pointer processed")
        if log("desktop").count("INTENT 0 2 2 0\n") != 2:
            raise RuntimeError("device replacement unexpectedly activated a result")
        event("B 272 1")
        event("B 272 0")
        wait(lambda: log("desktop").count("INTENT 0 2 2 0\n") == 3, "replacement pointer click")
        if launch_owner:
            if launch_owner.recv(16) != b"L3T":
                raise RuntimeError("wrong terminal re-request packet")
            launch_owner.sendall(b"E3T")
            wait(lambda: log("desktop").count("LAUNCH_REPLY E T\n") >= 2, "second terminal rejection")
            event("M 700 1030")
            event("B 272 1")
            event("B 272 0")
            if launch_owner.recv(16) != b"L3S":
                raise RuntimeError("wrong settings launch packet")
            launch_owner.sendall(b"E3S")
            wait(lambda: "LAUNCH_REPLY E S\n" in log("desktop"), "settings rejection applied")
            result["launch_channel"] = "passed"
        if app_client:
            app = start(app_client, "app", ("--input",), (), env)
            wait(lambda: "Aetheri protocol test A | " in log("desktop") and
                 "Aetheri protocol test B | " in log("desktop"), "real observed windows")
            window_a = int(re.search(r"^WINDOW (\d+) \d+ Aetheri protocol test A \|", log("desktop"), re.M)[1])
            window_b = int(re.search(r"^WINDOW (\d+) \d+ Aetheri protocol test B \|", log("desktop"), re.M)[1])
            command(client, "z")
            wait(lambda: "SYNC z\n" in log("desktop"), "unfiltered model")
            command(client, "w")
            wait(lambda: "SYNC w\n" in log("desktop"), "window snapshot")
            first_snapshot = log("desktop").split("SYNC z\n", 1)[1].split("SYNC w\n", 1)[0]
            for output in range(3):
                for window in (window_a, window_b):
                    if f"MODEL {output} {window} Aetheri protocol test " not in first_snapshot:
                        raise RuntimeError("observed window missing on output")
            row = re.search(rf"^WINDOW_ROW {window_a} (-?\d+\.\d+) Aetheri protocol test A$", first_snapshot, re.M)
            if not row:
                raise RuntimeError("observed window missing from view semantics")
            command(app, "u")
            wait(lambda: f"WINDOW {window_a} " in log("desktop") and "Grüße " in log("desktop"),
                 "window title update")
            command(client, "w")
            wait(lambda: log("desktop").count("SYNC w\n") >= 2, "updated title snapshot")
            title_snapshot = log("desktop").split("SYNC w\n", 1)[1].split("SYNC w\n", 1)[0]
            for output in range(3):
                if f"MODEL {output} {window_a} Grüße " not in title_snapshot:
                    raise RuntimeError("window title update missing on output")
            if not re.search(rf"^WINDOW_ROW {window_a} -?\d+\.\d+ Grüße ", title_snapshot, re.M):
                raise RuntimeError("view did not publish the updated title")
            command(client, "a")
            wait(lambda: f"ACTIVATED {window_a} 0\n" in log("desktop"), "exclusive-search activation denial")
            command(app, "d")
            wait(lambda: f"CLOSED_WINDOW {window_b}\n" in log("desktop"), "window close observation")
            command(client, "w")
            wait(lambda: log("desktop").count("SYNC w\n") >= 3, "updated window snapshot")
            second_snapshot = log("desktop").split("SYNC w\n", 2)[2].split("SYNC w\n", 1)[0]
            for output in range(3):
                if f"MODEL {output} {window_b} " in second_snapshot:
                    raise RuntimeError("closed window retained by model")
            if f"WINDOW_ROW {window_b} " in second_snapshot:
                raise RuntimeError("closed window retained by view")
            command(app, "c")
            def replacement_ids():
                return [int(value) for value in re.findall(
                    r"^WINDOW (\d+) \d+ Aetheri protocol test B \|", log("desktop"), re.M)]
            wait(lambda: any(value != window_b for value in replacement_ids()), "replacement window observation")
            replacement = next(value for value in replacement_ids() if value != window_b)
            if replacement == window_a:
                raise RuntimeError("replacement reused another live window ID")
            command(client, "w")
            wait(lambda: log("desktop").count("SYNC w\n") >= 4, "replacement window snapshot")
            third_snapshot = log("desktop").split("SYNC w\n", 3)[3].split("SYNC w\n", 1)[0]
            for output in range(3):
                if f"MODEL {output} {window_b} " in third_snapshot or (
                        f"MODEL {output} {replacement} Aetheri protocol test B" not in third_snapshot):
                    raise RuntimeError("replacement window identity was not preserved")
            if f"WINDOW_ROW {window_b} " in third_snapshot or (
                    f"WINDOW_ROW {replacement} " not in third_snapshot):
                raise RuntimeError("view did not replace the closed window")
            stale_case = args.test_stale_activation or args.test_stale_after_collapse
            target = replacement if stale_case else window_a
            row = re.search(rf"^WINDOW_ROW {target} (-?\d+\.\d+) ", third_snapshot, re.M)
            if not row:
                raise RuntimeError("target window missing from updated view")
            event(f"M 60 {int(float(row[1]) + 20)}")
            event("B 272 1")
            event("B 272 0")
            wait(lambda: f"INTENT 0 3 0 {target}\n" in log("desktop"), "observed window intent")
            if stale_case:
                if args.test_stale_after_collapse:
                    wait(lambda: f"ACTIVATION_HELD {replacement}\n" in log("desktop") and
                         log("desktop").count("PAINT 0 1920 84\n") >= 2, "owner waiting after collapse")
                command(app, "d")
                wait(lambda: f"CLOSED_WINDOW {replacement}\n" in log("desktop"), "target closed before validation")
                command(client, "g")
                wait(lambda: f"ACTIVATION_CANCELLED {replacement}\n" in log("desktop"), "stale activation cancelled")
                if f"ACTIVATION_REQUEST {replacement}\n" in log("desktop"):
                    raise RuntimeError("closed window was sent for activation")
                if args.test_stale_activation:
                    command(client, "n")
            else:
                wait(lambda: f"ACTIVATION_REQUEST {window_a}\n" in log("desktop") and
                     f"ACTIVATED {window_a} 1\n" in log("desktop"), "clicked window activation")
            wait(lambda: log("desktop").count("PAINT 0 1920 84\n") >= 2, "collapse before activation")
            expected_exits.add(app)
            command(app, "q")
            if app.wait(timeout=10):
                raise RuntimeError("app cleanup failed")
            wait(lambda: f"CLOSED_WINDOW {window_a}\n" in log("desktop") and
                 f"CLOSED_WINDOW {replacement}\n" in log("desktop"), "app removal")
            command(client, "e")
            wait(lambda: log("desktop").count("PAINT 0 1920 1080\n") >= 2, "reopen search")
            result["window_observation"] = "passed"
            result["observed_window_ids"] = [window_a, window_b, replacement]
            result["activation_case"] = (
                "stale-after-collapse" if args.test_stale_after_collapse else
                "stale-before-validation" if args.test_stale_activation else "clicked-window")
        command(client, "f")
        wait(lambda: "SYNC f\n" in log("desktop"), "long results model")
        command(client, "i")
        wait(lambda: len(re.findall(r"^ROW 20 (-?\d+\.\d+)$", log("desktop"), re.M)) == 1,
             "initial row position")
        before = float(re.findall(r"^ROW 20 (-?\d+\.\d+)$", log("desktop"), re.M)[0])
        event("M 60 250")
        for _ in range(10):
            event("S 10")
        command(client, "i")
        wait(lambda: len(re.findall(r"^ROW 20 (-?\d+\.\d+)$", log("desktop"), re.M)) == 2,
             "scrolled row position")
        after = float(re.findall(r"^ROW 20 (-?\d+\.\d+)$", log("desktop"), re.M)[1])
        if not (before - after >= 300 and 68 <= after < 940):
            raise RuntimeError(f"unexpected scroll position {before} -> {after}")
        result["scroll_before_y"], result["scroll_after_y"] = before, after
        event(f"M 60 {int(after + 20)}")
        event("B 272 1")
        event("B 272 0")
        wait(lambda: "INTENT 0 3 0 20\n" in log("desktop"), "scrolled stable window intent")
        for _ in range(3):
            event("S -10")
        command(client, "i")
        wait(lambda: len(re.findall(r"^ROW 20 (-?\d+\.\d+)$", log("desktop"), re.M)) == 3,
             "reverse scroll position")
        reverse = float(re.findall(r"^ROW 20 (-?\d+\.\d+)$", log("desktop"), re.M)[2])
        if reverse - after < 80:
            raise RuntimeError(f"reverse scroll did not restore position: {after} -> {reverse}")
        event("M 10 20")
        event("S 10")
        command(client, "i")
        wait(lambda: len(re.findall(r"^ROW 20 (-?\d+\.\d+)$", log("desktop"), re.M)) == 4,
             "outside scroll position")
        outside = float(re.findall(r"^ROW 20 (-?\d+\.\d+)$", log("desktop"), re.M)[3])
        if outside != reverse:
            raise RuntimeError("scroll outside results moved the list")
        result["scroll_reverse_y"], result["scroll_outside_y"] = reverse, outside
        command(client, "n")
        expected_collapses = 3 if app_client else 2
        wait(lambda: log("desktop").count("PAINT 0 1920 84\n") >= expected_collapses, "collapsed dock")
        command(client, "q")
        if client.wait(timeout=10) or compositor.wait(timeout=10):
            raise RuntimeError("retirement failed")
        if "RETIRED\n" not in log("desktop"):
            raise RuntimeError("missing retirement acknowledgement")
        result["status"] = "passed"
    finally:
        for child in reversed(processes):
            if child.poll() is None:
                child.terminate()
                try:
                    child.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    child.kill()
                    child.wait(timeout=5)
            if child.stdin:
                child.stdin.close()
        for stream in streams:
            stream.close()
        for sock in (controller, issuer, shell_server, shell_client, launch_owner, launch_client):
            if sock:
                sock.close()
        # Captures are owned by this fixture; keep them without exhausting
        # the test user's temporary-storage quota over repeated runs.
        for image in captures.glob("*.ppm"):
            with image.open("rb") as source, gzip.open(str(image) + ".gz", "wb") as target:
                shutil.copyfileobj(source, target)
            image.unlink()
        result["exit_codes"] = [p.returncode for p in processes]
        (root / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
