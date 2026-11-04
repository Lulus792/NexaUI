#!/usr/bin/env python3
"""Build an external C17 add_subdirectory consumer in four Linux profiles.

This exercises public core APIs, not native display or Vulkan presentation.
The system must provide the selected Wayland and (when enabled) Vulkan SDKs.
X11 CMake discovery is forbidden, but its headers/libraries are not masked.
"""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


SOURCE = Path(__file__).resolve().parents[1]


def run(*args, env=None):
    print('+', *map(str, args), flush=True)
    subprocess.run(args, check=True, env=env)


def main():
    with tempfile.TemporaryDirectory(prefix='nexaui-subdirectory-consumer-') as tmp:
        root = Path(tmp)
        consumer = root / 'external-consumer'
        consumer.mkdir()
        shutil.copyfile(SOURCE / 'tests/package_consumer/main.c', consumer / 'main.c')
        (consumer / 'CMakeLists.txt').write_text('''\
cmake_minimum_required(VERSION 3.21)
project(ExternalNexaUIConsumer LANGUAGES C)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
add_subdirectory("${NEXAUI_SOURCE_DIR}" nexaui-build)
add_executable(consumer main.c)
target_link_libraries(consumer PRIVATE NexaUI::NexaUI)
''', encoding='utf-8')
        for shared in ('OFF', 'ON'):
            for vulkan in ('OFF', 'ON'):
                build = root / f'build-shared-{shared}-vulkan-{vulkan}'
                run('cmake', '-S', consumer, '-B', build,
                    '-DCMAKE_BUILD_TYPE=Release',
                    '-DCMAKE_DISABLE_FIND_PACKAGE_X11=ON',
                    '-DNEXAUI_ENABLE_X11=OFF',
                    '-DNEXAUI_BUILD_TESTS=OFF',
                    '-DNEXAUI_BUILD_EXAMPLES=OFF',
                    '-DNEXAUI_BUILD_VULKAN=' + vulkan,
                    '-DBUILD_SHARED_LIBS=' + shared,
                    '-DNEXAUI_SOURCE_DIR=' + str(SOURCE))
                run('cmake', '--build', build, '--parallel', '4')
                executable = build / 'consumer'
                env = os.environ.copy()
                env.pop('LD_LIBRARY_PATH', None)
                if shared == 'ON':
                    # Check resolution, not just a successful run against some
                    # other installed version with the same SONAME.
                    resolved = subprocess.run(['ldd', str(executable)], env=env,
                                              check=True, text=True,
                                              capture_output=True).stdout
                    print(resolved, flush=True)
                    matches = [line.split('=>', 1)[1].strip().split()[0]
                               for line in resolved.splitlines()
                               if line.strip().startswith('libnexaui.so') and '=>' in line]
                    if len(matches) != 1 or not Path(matches[0]).resolve().is_relative_to(build.resolve()):
                        raise RuntimeError('consumer did not resolve this build of NexaUI')
                run(executable, env=env)
    print('Four external add_subdirectory C17 consumers passed.')


if __name__ == '__main__':
    main()
