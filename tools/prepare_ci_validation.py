#!/usr/bin/env python3
"""Prepare a pinned, present-fence-aware validation layer; keep system headers."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
from urllib.request import urlopen

VERSION = '1.4.313'
URL = ('https://packages.lunarg.com/vulkan/1.4.313/pool/main/v/vulkan-validationlayers/'
       'vulkan-validationlayers_1.4.313.0~rc2-1lunarg24.04-1_amd64.deb')
SHA256 = 'b100bfafac3df98c5d8ef5a572b423bcd9ebf3fab963e2e68957f4bf6f2423f0'


def deb_data(archive):
    if archive[:8] != b'!<arch>\n':
        raise ValueError('Not a Debian ar archive')
    offset = 8
    while offset + 60 <= len(archive):
        header = archive[offset:offset+60]
        if header[58:60] != b'`\n':
            raise ValueError('Invalid ar member header')
        length = int(header[48:58])
        start = offset + 60
        if length < 0 or start + length > len(archive):
            raise ValueError('Invalid ar member size')
        if header[:16].strip().rstrip(b'/') == b'data.tar.zst':
            return archive[start:start+length]
        offset = start + length + length % 2
    raise ValueError('Pinned data.tar.zst member is absent')


def prepare(destination, archive_path=None):
    destination = destination.resolve()
    destination.mkdir(parents=True, exist_ok=True)
    if archive_path:
        archive = archive_path.read_bytes()
    else:
        with urlopen(URL, timeout=30) as response:
            archive = response.read(8 * 1024 * 1024 + 1)
    if hashlib.sha256(archive).hexdigest() != SHA256:
        raise ValueError('Validation-layer package SHA-256 mismatch')
    data = destination / 'layer-data.tar.zst'
    data.write_bytes(deb_data(archive))
    # Read only two named members; do not extract package paths or run scripts.
    library = subprocess.check_output(['tar', '-xOf', str(data),
        './usr/lib/x86_64-linux-gnu/libVkLayer_khronos_validation.so'])
    manifest = json.loads(subprocess.check_output(['tar', '-xOf', str(data),
        './usr/share/vulkan/explicit_layer.d/VkLayer_khronos_validation.json']))
    if manifest['layer']['name'] != 'VK_LAYER_KHRONOS_validation' or manifest['layer']['api_version'] != VERSION:
        raise ValueError('Unexpected validation layer manifest')
    library_path = destination / 'libVkLayer_khronos_validation.so'
    library_path.write_bytes(library)
    manifest['layer']['library_path'] = library_path.as_posix()
    (destination / 'VkLayer_khronos_validation.json').write_text(
        json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    print(f'Prepared Khronos validation {VERSION}, SHA-256 {SHA256}; system SDK/loader unchanged.')
    return destination


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('destination', type=Path)
    parser.add_argument('--archive', type=Path, help='Use an already downloaded package; hash still required')
    parser.add_argument('--github-env', type=Path)
    args = parser.parse_args()
    directory = prepare(args.destination, args.archive)
    if args.github_env:
        with args.github_env.open('a', encoding='utf-8') as output:
            output.write(f'VK_LAYER_PATH={directory.as_posix()}\n')
            output.write('NEXAUI_REQUIRE_VALIDATION=1\n')
            output.write(f'NEXAUI_CI_VALIDATION_VERSION={VERSION}\n')


if __name__ == '__main__':
    main()
