"""Run the owner fixture and independent client on a private AT-SPI bus."""
import argparse
import os
from pathlib import Path
import subprocess
import sys
import time
from xml.sax.saxutils import escape


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("dbus", "registry", "runner", "client", "bus-dir"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--registry-library-dir", type=Path)
    parser.add_argument("owner_args", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    extra = args.owner_args[1:] if args.owner_args[:1] == ["--"] else args.owner_args
    root = args.bus_dir.resolve()
    root.mkdir(mode=0o700)
    address = "unix:path=" + str(root / "bus")
    conf = root / "bus.conf"
    conf.write_text("<busconfig><type>session</type><listen>" + escape(address) + "</listen>"
                    "<auth>EXTERNAL</auth><policy context=\"default\"><allow user=\"" + str(os.getuid()) + "\"/>"
                    "<allow own=\"*\"/><allow send_destination=\"*\"/><allow receive_sender=\"*\"/>"
                    "</policy></busconfig>\n")
    env = os.environ.copy()
    env.update(DBUS_SESSION_BUS_ADDRESS=address, AT_SPI_BUS_ADDRESS=address,
               NEXAUI_TEST_ATSPI_ADDRESS=address, XDG_RUNTIME_DIR=str(root), GSETTINGS_BACKEND="memory")
    for key in ("NO_AT_BRIDGE", "DBUS_STARTER_ADDRESS", "DBUS_STARTER_BUS_TYPE"):
        env.pop(key, None)
    processes = []
    try:
        with (root / "bus.log").open("wb") as bus_log, (root / "registry.log").open("wb") as registry_log:
            bus = subprocess.Popen([str(args.dbus.resolve(strict=True)), "--nofork", "--config-file=" + str(conf)],
                                   env=env, stdout=bus_log, stderr=subprocess.STDOUT)
            processes.append(bus)
            deadline = time.monotonic() + 10
            while not (root / "bus").is_socket():
                if bus.poll() is not None or time.monotonic() >= deadline:
                    raise RuntimeError("private bus failed to start")
                time.sleep(.02)
            registry_env = env.copy()
            if args.registry_library_dir:
                registry_env["LD_LIBRARY_PATH"] = str(args.registry_library_dir.resolve(strict=True))
            registry = subprocess.Popen([str(args.registry.resolve(strict=True))], env=registry_env,
                                        stdout=registry_log, stderr=subprocess.STDOUT)
            processes.append(registry)
            while time.monotonic() < deadline:
                status = subprocess.run(["busctl", "--address=" + address, "--timeout=1", "call",
                                         "org.freedesktop.DBus", "/org/freedesktop/DBus",
                                         "org.freedesktop.DBus", "NameHasOwner", "s", "org.a11y.atspi.Registry"],
                                        env=env, capture_output=True, text=True, timeout=2)
                if status.returncode == 0 and status.stdout.strip() == "b true":
                    break
                if registry.poll() is not None:
                    raise RuntimeError("private AT-SPI registry exited")
                time.sleep(.02)
            else:
                raise RuntimeError("private AT-SPI registry did not register")
            return subprocess.run([sys.executable, str(args.runner.resolve(strict=True)),
                                   "--atspi-client", str(args.client.resolve(strict=True)), *extra],
                                  env=env, timeout=120).returncode
    finally:
        for process in reversed(processes):
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()


if __name__ == "__main__":
    raise SystemExit(main())
