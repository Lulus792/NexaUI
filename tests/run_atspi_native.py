"""Run an independent AT-SPI client against a real, private Linux registry."""
import os
import select
import signal
import re
import subprocess
import sys
import tempfile
import time


def stop(process):
    if process is None:
        return
    # Only sessions created by this fixture are targeted. The launcher may own
    # its private accessibility dbus-daemon; the client may still own its host.
    try:
        os.killpg(process.pid, signal.SIGTERM)
    except ProcessLookupError:
        pass
    try:
        process.wait(timeout=2)
    except subprocess.TimeoutExpired:
        os.killpg(process.pid, signal.SIGKILL)
        process.wait()


def rpc(address, destination, path, method, *arguments):
    return subprocess.run(["dbus-send", f"--bus={address}", "--type=method_call",
                           "--print-reply", f"--dest={destination}", path, method,
                           *arguments], capture_output=True, text=True, timeout=2)


def main():
    if len(sys.argv) not in (4, 5):
        return 2
    registry, client, host = sys.argv[1:4]
    discovery = len(sys.argv) == 5
    with tempfile.TemporaryDirectory(prefix="nexaui-atspi-") as directory:
        address = f"unix:path={directory}/bus"
        bus_env = os.environ.copy()
        bus_env["XDG_RUNTIME_DIR"] = directory
        bus_env["GSETTINGS_BACKEND"] = "memory"
        bus = subprocess.Popen(["dbus-daemon", "--session", "--nofork",
                                f"--address={address}", "--print-address=1"],
                               stdout=subprocess.PIPE, text=True, env=bus_env,
                               start_new_session=True)
        daemon = launcher = client_process = None
        try:
            if not select.select([bus.stdout], [], [], 10)[0]:
                raise RuntimeError("private D-Bus startup timed out")
            if bus.stdout.readline().strip() != address:
                # The daemon may append its GUID to the effective address.
                if bus.poll() is not None:
                    raise RuntimeError("private D-Bus startup failed")
            env = os.environ.copy()
            env.update(DBUS_SESSION_BUS_ADDRESS=address, AT_SPI_BUS_ADDRESS=address)
            env.pop("NO_AT_BRIDGE", None)
            if discovery:
                launcher_env = env.copy()
                launcher_env.pop("AT_SPI_BUS_ADDRESS", None)
                launcher_env.update(XDG_RUNTIME_DIR=directory, GSETTINGS_BACKEND="memory")
                launcher = subprocess.Popen([sys.argv[4], "--launch-immediately", "--a11y=1"],
                                            env=launcher_env, start_new_session=True)
                deadline = time.monotonic() + 5
                while time.monotonic() < deadline:
                    owner = rpc(address, "org.freedesktop.DBus", "/org/freedesktop/DBus",
                                "org.freedesktop.DBus.NameHasOwner", "string:org.a11y.Bus")
                    if owner.returncode == 0 and "boolean true" in owner.stdout:
                        break
                    time.sleep(0.01)
                else:
                    raise RuntimeError("accessibility launcher startup timed out")
                deadline = time.monotonic() + 5
                while time.monotonic() < deadline:
                    reply = rpc(address, "org.a11y.Bus", "/org/a11y/bus", "org.a11y.Bus.GetAddress")
                    match = re.search(r'string "([^"\n]+)"', reply.stdout)
                    if reply.returncode == 0 and match:
                        address = match.group(1)
                        break
                    time.sleep(0.01)
                else:
                    raise RuntimeError("accessibility bus discovery timed out")
                env.pop("AT_SPI_BUS_ADDRESS", None)
            env["NEXAUI_TEST_ATSPI_ADDRESS"] = address
            registry_env = env.copy()
            registry_env.update(XDG_RUNTIME_DIR=directory, AT_SPI_BUS_ADDRESS=address)
            daemon = subprocess.Popen([registry], env=registry_env, start_new_session=True)
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline:
                reply = rpc(address, "org.freedesktop.DBus", "/org/freedesktop/DBus",
                            "org.freedesktop.DBus.NameHasOwner", "string:org.a11y.atspi.Registry")
                if reply.returncode == 0 and "boolean true" in reply.stdout:
                    break
                time.sleep(0.01)
            else:
                raise RuntimeError("AT-SPI registry startup timed out")
            arguments = [host, "--optional"] if client == "--optional" else [client, host]
            client_env = env.copy()
            if client == "--optional" and "NEXAUI_TEST_DBUS_LIBRARY_DIR" in env:
                client_env["LD_LIBRARY_PATH"] = env["NEXAUI_TEST_DBUS_LIBRARY_DIR"]
            client_process = subprocess.Popen(arguments, env=client_env, start_new_session=True)
            result = client_process.wait(timeout=35)
            if result:
                print(f"AT-SPI client exited with {result}", file=sys.stderr)
            return result
        finally:
            stop(client_process)
            stop(daemon)
            stop(launcher)
            stop(bus)


if __name__ == "__main__":
    raise SystemExit(main())
