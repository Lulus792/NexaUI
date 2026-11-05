#!/usr/bin/env python3
"""Build a copied Settings consumer and exercise it on a private Wayland seat."""
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def run(command, env):
    print('+', *map(str, command), flush=True)
    subprocess.run(command, env=env, check=True, timeout=90)


def main():
    if len(sys.argv) != 7:
        print('usage: check_installed_settings.py PREFIX FONT XDOTOOL ATSPI_REGISTRY ATSPI_CLIENT REVISION', file=sys.stderr)
        return 2
    if (os.environ.get('NEXAUI_TEST_PRIVATE_WAYLAND') != '1' or
            os.environ.get('WAYLAND_DISPLAY') != 'nexaui-test'):
        print('Requires the private Weston fixture.')
        return 77
    prefix, font, xdotool, registry, client = map(Path, sys.argv[1:6])
    revision = sys.argv[6].lower()
    prefix = prefix.resolve(strict=True)
    env = os.environ.copy()
    env['LD_LIBRARY_PATH'] = os.pathsep.join([str(prefix / 'lib'), str(prefix / 'lib64'),
                                              env.get('LD_LIBRARY_PATH', '')])
    env['NEXAUI_PLATFORM'] = 'wayland'
    no_atspi = env.get('NEXAUI_TEST_SETTINGS_NO_ATSPI') == '1'
    if no_atspi:
        # Fail closed even if the host session happens to provide a bus.
        env['AT_SPI_BUS_ADDRESS'] = 'unix:path=/nonexistent/nexaui-atspi-bus'
        env['DBUS_SESSION_BUS_ADDRESS'] = 'unix:path=/nonexistent/nexaui-session-bus'
    with tempfile.TemporaryDirectory(prefix='nexaui-external-settings-') as tmp:
        root = Path(tmp)
        configs = list(prefix.glob('lib*/cmake/NexaUI/NexaUIConfig.cmake'))
        if len(configs) != 1 or not re.fullmatch('[0-9a-f]{40}|[0-9a-f]{64}', revision):
            raise RuntimeError('Missing package configuration or invalid source revision')
        if f'set(NexaUI_SOURCE_REVISION "{revision}")' not in configs[0].read_text(encoding='utf-8'):
            raise RuntimeError('Installed package declares a different source revision')
        consumer_build = root / 'package-consumer'
        run(['cmake', '-S', ROOT / 'tests/package_consumer', '-B', consumer_build,
             '-DCMAKE_DISABLE_FIND_PACKAGE_X11=ON', '-DCMAKE_PREFIX_PATH=' + str(prefix)], env)
        run(['cmake', '--build', consumer_build, '--parallel', '4'], env)
        run([consumer_build / 'consumer', '--expect-wayland-only-no-atspi' if no_atspi else '--expect-wayland-only'], env)
        source, build, install = root / 'source', root / 'build', root / 'app'
        source.mkdir()
        for name in ('CMakeLists.txt', 'main_linux.c', 'renderer.c', 'renderer.h', 'scene.c', 'scene.h'):
            shutil.copy2(ROOT / 'examples/18_settings' / name, source / name)
        software = env.get('NEXAUI_TEST_SETTINGS_SOFTWARE') == '1'
        no_vulkan = env.get('NEXAUI_TEST_SETTINGS_NO_VULKAN_PACKAGE') == '1'
        optional = ['-DCMAKE_DISABLE_FIND_PACKAGE_Vulkan=ON'] if software or no_vulkan else []
        run(['cmake', '-S', source, '-B', build, '-DCMAKE_BUILD_TYPE=Release',
             '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON', '-DCMAKE_DISABLE_FIND_PACKAGE_X11=ON',
             '-DCMAKE_PREFIX_PATH=' + str(prefix),
             '-DNEXAUI_SETTINGS_SOFTWARE=' + ('ON' if software else 'OFF'), *optional], env)
        run(['cmake', '--build', build, '--parallel', '4'], env)
        commands = json.loads((build / 'compile_commands.json').read_text(encoding='utf-8'))
        if len(commands) != 3 or any(str(ROOT) in item['command'] or
                                     not Path(item['file']).is_relative_to(source) for item in commands):
            raise RuntimeError('Consumer unexpectedly uses a source-tree path')
        run(['cmake', '--install', build, '--prefix', install], env)
        app = install / 'bin/nexaui_settings'
        libraries = subprocess.run(['ldd', str(app)], env=env, text=True,
                                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                   check=True, timeout=15).stdout
        print(libraries, flush=True)
        if 'not found' in libraries or str(ROOT) in libraries:
            raise RuntimeError('Consumer resolved a missing or source-tree library')
        resolved = re.findall(r'^\s*libnexaui\.so\S*\s+=>\s+(\S+)', libraries, re.MULTILINE)
        shared = list(prefix.glob('lib*/libnexaui.so*'))
        if shared and (len(resolved) != 1 or not Path(resolved[0]).resolve().is_relative_to(prefix)):
            raise RuntimeError('Shared consumer did not load the requested SDK prefix')
        if not shared and resolved:
            raise RuntimeError('Static consumer unexpectedly loaded a shared SDK')
        if (software or no_vulkan) and re.search(r'^\s*libvulkan\.so', libraries, re.MULTILINE):
            raise RuntimeError('Software consumer unexpectedly depends on Vulkan')
        if no_atspi and re.search(r'^\s*(libdbus|libatspi|libglib|libgobject)\S*\s+=>', libraries, re.MULTILINE):
            raise RuntimeError('Accessibility-disabled consumer unexpectedly links a desktop accessibility library')
        if no_atspi and re.search(r'^\s*(libX11|libXext)\S*\s+=>', libraries, re.MULTILINE):
            raise RuntimeError('Wayland-only consumer unexpectedly links an X11 library')
        if env.get('NEXAUI_TEST_SIMPLE_IME') == '1':
            run([sys.executable, ROOT / 'examples/18_settings/run_simple_ime.py', app, font, xdotool], env)
            print('Copied installed Settings consumer passed native IME composition.')
            return 0
        run([sys.executable, ROOT / 'examples/18_settings/run_keyboard.py', app, font, xdotool, '--small'], env)
        if not no_atspi:
            ax_env = dict(env, NEXAUI_SETTINGS_FONT=str(font), NEXAUI_SETTINGS_TEST_APPLY_ERROR='1')
            run([sys.executable, ROOT / 'tests/run_atspi_multiselection_wayland.py', registry, client, app], ax_env)
    if no_atspi:
        print('Copied installed Settings consumer passed native keyboard with AT-SPI explicitly disabled; no accessibility support claimed.')
    else:
        print('Copied installed Settings consumer passed native keyboard and AT-SPI retry.')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
