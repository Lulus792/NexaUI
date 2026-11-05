"""Exercise the NexaUI model view on Aetheri's isolated test shell connection."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import signal
import socket
import subprocess
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("server", "desktop", "font", "work-dir"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    server, desktop, font = (p.resolve(strict=True) for p in (args.server, args.desktop, args.font))
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
    processes, streams = [], []
    def digest(path):
        with path.open("rb") as stream:
            return hashlib.file_digest(stream, "sha256").hexdigest()

    result = {"status": "failed", "scope": "isolated Aetheri test compositor, virtual outputs",
              "server_sha256": digest(server), "desktop_sha256": digest(desktop),
              "font_sha256": digest(font)}
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
                if any(p.poll() is not None for p in processes):
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
        client = start(desktop, "desktop", (), (shell_client.fileno(),),
                       dict(env, WAYLAND_SOCKET=str(shell_client.fileno())))
        shell_client.close()
        wait(lambda: all(f"PAINT {i} {width} 84\n" in log("desktop")
                         for i, width in enumerate((1920, 2560, 3840))), "three docks")
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
            offset = ((height - 65) * width + 20) * 3
            if tuple(pixels[offset:offset + 3]) != (128, 175, 255):
                raise RuntimeError("dock pixels missing on output " + str(index))
            result[f"dock_output_{index}_sha256"] = hashlib.sha256(raw).hexdigest()
        command(client, "e")
        wait(lambda: "PAINT 0 1920 1080\n" in log("desktop"), "expanded search")
        search = capture(2)
        result["search_field"] = search(20, 20)
        if search(20, 20) != (38, 44, 54):
            raise RuntimeError("search field pixels missing")
        command(client, "n")
        wait(lambda: log("desktop").count("PAINT 0 1920 84\n") >= 2, "collapsed dock")
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
        for sock in (controller, issuer, shell_server, shell_client):
            sock.close()
        result["exit_codes"] = [p.returncode for p in processes]
        (root / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
