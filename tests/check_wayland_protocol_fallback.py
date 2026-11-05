#!/usr/bin/env python3
"""Build a Wayland-only SDK against protocol data without fractional-scale-v1."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

source = Path(__file__).resolve().parents[1]
protocols = Path(subprocess.check_output(
    ["pkg-config", "--variable=pkgdatadir", "wayland-protocols"], text=True).strip())

with tempfile.TemporaryDirectory(prefix="nexaui-old-wayland-protocols-") as temporary:
    root = Path(temporary)
    data = root / "protocols"
    for relative in (
        "stable/xdg-shell/xdg-shell.xml",
        "stable/viewporter/viewporter.xml",
        "unstable/xdg-decoration/xdg-decoration-unstable-v1.xml",
        "unstable/text-input/text-input-unstable-v1.xml",
        "unstable/text-input/text-input-unstable-v3.xml",
    ):
        destination = data / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(protocols / relative, destination)
    pc = root / "pkgconfig"
    pc.mkdir()
    (pc / "wayland-protocols.pc").write_text(
        f"pkgdatadir={data}\nName: Wayland Protocols\n"
        "Description: Isolated protocol fallback fixture\nVersion: 1.25\n",
        encoding="utf-8")
    env = os.environ.copy()
    env["PKG_CONFIG_PATH"] = str(pc) + os.pathsep + env.get("PKG_CONFIG_PATH", "")
    env.setdefault("CMAKE_BUILD_PARALLEL_LEVEL", "6")
    build = root / "build"
    subprocess.run(["cmake", "-S", str(source), "-B", str(build),
                    "-DNEXAUI_ENABLE_X11=OFF", "-DNEXAUI_BUILD_VULKAN=OFF",
                    "-DNEXAUI_BUILD_TESTS=ON", "-DNEXAUI_BUILD_EXAMPLES=OFF",
                    "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"], check=True, env=env)
    commands = json.loads((build / "compile_commands.json").read_text(encoding="utf-8"))
    wayland = [entry for entry in commands if entry["file"].endswith("/platform/linux/wayland/window.c")]
    assert len(wayland) == 1 and "NEXAUI_WAYLAND_FRACTIONAL_SCALE" not in wayland[0]["command"]
    subprocess.run(["cmake", "--build", str(build), "--target", "nexaui_wayland_async_clipboard_tests",
                    "nexaui_build_features_tests"],
                   check=True, env=env)
    subprocess.run(["ctest", "--test-dir", str(build), "-R", "^(nexaui_wayland_async_clipboard|nexaui_build_features)$",
                    "--output-on-failure"], check=True, env=env)
print("Wayland SDK builds and runs its controlled clipboard test without fractional protocol XML.")
