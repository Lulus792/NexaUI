#!/usr/bin/env python3
"""B01 acceptance probe; run manually on Linux with the selected SDK dependencies.

Uses fresh temporary build/install/consumer directories. It intentionally forbids
CMake's X11 package in both SDK and consumer configurations. A passing run is
not native compositor, IME, accessibility, or proof that X11 headers are absent
from the machine; those require separate checks in a minimal build environment.
"""
import os
from pathlib import Path
import subprocess
import tempfile

SOURCE = Path(__file__).resolve().parents[1]


def run(*args, env=None):
    print("+", *map(str, args), flush=True)
    subprocess.run(args, check=True, env=env)


def main():
    with tempfile.TemporaryDirectory(prefix="nexaui-wayland-profile-") as tmp:
        root = Path(tmp)
        for shared in ("OFF", "ON"):
            build = root / ("sdk-" + shared)
            prefix = root / ("install-" + shared)
            consumer = root / ("consumer-" + shared)
            run("cmake", "-S", str(SOURCE), "-B", str(build),
                "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_DISABLE_FIND_PACKAGE_X11=ON",
                "-DNEXAUI_ENABLE_X11=OFF", "-DNEXAUI_BUILD_VULKAN=OFF",
                "-DNEXAUI_BUILD_TESTS=OFF", "-DNEXAUI_BUILD_EXAMPLES=OFF",
                "-DBUILD_SHARED_LIBS=" + shared, "-DCMAKE_INSTALL_PREFIX=" + str(prefix))
            run("cmake", "--build", str(build))
            run("cmake", "--install", str(build))
            run("cmake", "-S", str(SOURCE / "tests/package_consumer"), "-B", str(consumer),
                "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_DISABLE_FIND_PACKAGE_X11=ON",
                "-DCMAKE_C_STANDARD=17", "-DCMAKE_C_STANDARD_REQUIRED=ON",
                "-DCMAKE_C_EXTENSIONS=OFF", "-DCMAKE_PREFIX_PATH=" + str(prefix))
            run("cmake", "--build", str(consumer))
            env = os.environ.copy()
            # On Linux the shared consumer must load the just-installed SDK.
            libdirs = [str(prefix / "lib"), str(prefix / "lib64")]
            env["LD_LIBRARY_PATH"] = os.pathsep.join(libdirs + [env.get("LD_LIBRARY_PATH", "")])
            run(str(consumer / "consumer"), env=env)
    print("Static and shared installed consumers passed without CMake X11 discovery.")


if __name__ == "__main__":
    main()
