"""Prepare an isolated copy for Aetheri's unchanged compositor test.

The original Aetheri test intentionally requires a reviewed source/version pin
and build metadata. A trial must supply real metadata for the candidate, without
editing Aetheri's checkout or claiming that its owner approved a new pin.
"""
import argparse
import ctypes
import hashlib
import json
from pathlib import Path
import re
import shutil


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--snapshot', type=Path, required=True)
    parser.add_argument('--test-copy', type=Path, required=True)
    parser.add_argument('--candidate', type=Path, required=True)
    parser.add_argument('--revision', required=True)
    args = parser.parse_args()
    snapshot = args.snapshot.resolve(strict=True)
    destination = args.test_copy.resolve()
    candidate = args.candidate.resolve(strict=True)
    if not re.fullmatch(r'[0-9a-f]{40}', args.revision):
        raise ValueError('Expected a full lower-case Git revision')
    if destination.exists() or destination.is_relative_to(snapshot) or snapshot.is_relative_to(destination):
        raise ValueError('Test copy must be a new independent directory')
    library = (candidate / 'prefix/lib/libnexaui.so.0.1.19').resolve(strict=True)
    if not library.is_relative_to(candidate) or (candidate / 'build.json').exists():
        raise ValueError('Candidate must own its library and have no existing build metadata')
    core = ctypes.CDLL(str(library))
    version = (ctypes.c_uint32 * 3)()
    revision = ctypes.c_char_p()
    features = ctypes.c_uint64()
    if (core.nx_get_version(ctypes.byref(version)) != 0 or tuple(version) != (0, 1, 19) or
            core.nx_get_source_revision(ctypes.byref(revision)) != 0 or
            revision.value.decode('ascii') != args.revision or
            core.nx_get_build_features(ctypes.byref(features)) != 0 or
            not (features.value & (1 << 2)) or features.value & (1 << 3) or
            not (features.value & (1 << 4))):
        raise ValueError('The linked candidate version, revision or Wayland/AT-SPI profile does not match')
    original_pin = snapshot / 'spec/nexaui-inputs.json'
    original_test = snapshot / 'tools/test_settings_view.py'
    pin = json.loads(original_pin.read_text(encoding='utf-8'))
    original_pin_hash = digest(original_pin)
    test_hash = digest(original_test)
    shutil.copytree(snapshot, destination, symlinks=False)
    pin.update(revision=args.revision, version='0.1.19')
    (destination / 'spec/nexaui-inputs.json').write_text(json.dumps(pin, indent=2) + '\n', encoding='utf-8')
    if digest(destination / 'tools/test_settings_view.py') != test_hash:
        raise ValueError('The copied original test changed')
    relative = library.relative_to(candidate).as_posix()
    metadata = {'revision': args.revision, 'version': '0.1.19', 'atspi_compiled': True,
                'artifacts': {relative: digest(library)}, 'scope': 'isolated compatibility trial only'}
    with (candidate / 'build.json').open('x', encoding='utf-8') as output:
        json.dump(metadata, output, indent=2)
        output.write('\n')
    record = {'scope': 'isolated candidate test; no Aetheri pin or owner change',
              'original_pin_sha256': original_pin_hash,
              'trial_pin_sha256': digest(destination / 'spec/nexaui-inputs.json'),
              'original_test_sha256': test_hash, 'library_sha256': digest(library),
              'declared_revision': args.revision, 'compiled_features': features.value}
    with (candidate / 'trial.json').open('x', encoding='utf-8') as output:
        json.dump(record, output, indent=2)
        output.write('\n')
    print(json.dumps(record, indent=2))


if __name__ == '__main__':
    main()
