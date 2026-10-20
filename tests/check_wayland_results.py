"""Require actual successful execution of every native Wayland CTest case."""
import argparse
from pathlib import Path
import xml.etree.ElementTree as ET

REQUIRED = (
    'nexaui_vulkan_wayland_present', 'nexaui_vulkan_wayland_draw',
    'nexaui_vulkan_wayland_timing', 'nexaui_vulkan_wayland_ring',
    'nexaui_wayland_window', 'nexaui_nexacloud_wayland_native_example',
    'nexaui_vulkan_wayland_example',
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('junit', type=Path)
    args = parser.parse_args()
    cases = list(ET.parse(args.junit).getroot().iter('testcase'))
    for name in REQUIRED:
        matching = [case for case in cases if case.get('name') == name]
        if len(matching) != 1:
            parser.error(f'{name}: expected one result, found {len(matching)}')
        case = matching[0]
        if case.get('status') not in (None, 'run') or any(case.find(tag) is not None for tag in ('failure', 'error', 'skipped')):
            parser.error(f'{name}: did not execute successfully (skips are not acceptance)')
    print('Native Wayland: all 7 required cases executed successfully; no skips.')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
