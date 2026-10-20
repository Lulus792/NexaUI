"""Fixed Vulkan lifetime controls, with leak detection and no retained DSO handles.

--app runs the two existing native-clear controls (requires X11/private Weston).
--control adds four independent Vulkan-only device/draw controls, without a display.
LD_DEBUG=files preserves glibc loader addresses. Runtime GNU build IDs and disk
hashes identify the actual driver; full CTest remains a separate required gate.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys


def disk_identity(path):
    """Read ELF64 little-endian PT_NOTE build ID, without loading the library."""
    resolved = Path(path).resolve(strict=True)
    data = resolved.read_bytes()
    if data[:6] != b'\x7fELF\x02\x01':
        raise ValueError('expected ELF64 little-endian module')
    phoff = struct.unpack_from('<Q', data, 32)[0]
    entsize, count = struct.unpack_from('<HH', data, 54)
    if entsize < 56 or phoff + entsize * count > len(data):
        raise ValueError('invalid ELF program header range')
    build_id = None
    for i in range(count):
        header = phoff + i * entsize
        if struct.unpack_from('<I', data, header)[0] != 4:
            continue
        offset = struct.unpack_from('<Q', data, header + 8)[0]
        size = struct.unpack_from('<Q', data, header + 32)[0]
        end = offset + size
        if end > len(data):
            raise ValueError('invalid ELF note range')
        while offset + 12 <= end:
            names, desc, kind = struct.unpack_from('<III', data, offset)
            offset += 12
            desc_at = offset + ((names + 3) & ~3)
            next_at = desc_at + ((desc + 3) & ~3)
            if next_at > end:
                raise ValueError('invalid ELF note size')
            if kind == 3 and data[offset:offset + names] == b'GNU\0':
                build_id = data[desc_at:desc_at + desc].hex()
            offset = next_at
    return dict(path=str(resolved), size=len(data), sha256=hashlib.sha256(data).hexdigest(),
                disk_build_id=build_id)


def control_modules(stdout, draw):
    phases, modules, errors, identities = [], [], [], {}
    for line in stdout.splitlines():
        if line.startswith('SNAPSHOT\t'):
            phases.append(line.split('\t', 1)[1])
        if not line.startswith('MODULE\t'):
            continue
        try:
            _, phase, path, base, extent, build_id = line.split('\t')
            item = dict(phase=phase, path=path, base=base, extent=extent,
                        runtime_build_id=build_id)
            if not build_id or int(extent, 16) <= 0 or int(base, 16) < 0:
                raise ValueError('invalid runtime module identity')
            if path not in identities:
                identities[path] = disk_identity(path)
            item.update(identities[path])
            item['build_id_matches_disk'] = build_id == item['disk_build_id']
            if not item['build_id_matches_disk']:
                errors.append('runtime/disk build ID mismatch: ' + path)
            modules.append(item)
        except (ValueError, OSError, struct.error) as error:
            errors.append(str(error))
    expected = ['device-created'] + (['draw-completed'] if draw else []) + [
        'device-destroyed', 'instance-destroyed']
    if phases != expected:
        errors.append('missing or reordered lifetime snapshots')
    lvp = [item for item in modules if 'libvulkan_lvp.so' in item['path']]
    if not any(item['phase'] == 'device-created' for item in lvp):
        errors.append('Lavapipe driver identity missing at device creation')
    return dict(snapshots=phases, modules=modules, identity_errors=errors,
                lvp_loaded_after_instance_destroy=any(
                    item['phase'] == 'instance-destroyed' for item in lvp))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--app', type=Path)
    parser.add_argument('--control', type=Path)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    if sys.platform != 'linux':
        parser.error('this diagnostic requires Linux')
    if not args.app and not args.control:
        parser.error('supply --app and/or --control')
    binaries = {}
    for name in ('app', 'control'):
        value = getattr(args, name)
        if value:
            value = value.resolve()
            if not value.is_file():
                parser.error(name + ' executable is missing')
            binaries[name] = dict(path=str(value), sha256=hashlib.sha256(value.read_bytes()).hexdigest())
    work = args.work_dir.resolve()
    work.mkdir(parents=True, exist_ok=False)
    try:
        libc = os.confstr('CS_GNU_LIBC_VERSION')
    except (ValueError, OSError):
        libc = None
    report = dict(scope='Fixed controls; leak detection enabled, no suppression. Full CTest is a separate gate.',
                  binaries=binaries, libc=libc, module_logging_supported=bool(libc), runs=[])
    fixture = Path(__file__).resolve().parents[1]/'tests/run_weston_native.py'
    cases = []
    for enabled in (True, False):
        suffix = 'explicit-validation' if enabled else 'without-explicit-validation'
        if args.app:
            cases.append((suffix, enabled, None))
        if args.control:
            for mode in ('device', 'draw'):
                cases.append(('pure-' + mode + '-' + suffix, enabled, mode))
    failed = False
    for name, enabled, mode in cases:
        env = os.environ.copy()
        for key in ('VK_INSTANCE_LAYERS', 'VK_LAYER_VALIDATE_SYNC', 'LD_DEBUG',
                    'LD_DEBUG_OUTPUT', 'NEXAUI_PLATFORM', 'NEXAUI_REQUIRE_VULKAN'):
            env.pop(key, None)
        env.update(ASAN_OPTIONS='halt_on_error=1:detect_leaks=1:fast_unwind_on_malloc='+('0' if libc else '1'),
                   LSAN_OPTIONS='report_objects=1', UBSAN_OPTIONS='halt_on_error=1')
        if libc:
            env['LD_DEBUG'] = 'files'
        if enabled:
            env.update(VK_INSTANCE_LAYERS='VK_LAYER_KHRONOS_validation', VK_LAYER_VALIDATE_SYNC='1')
        if mode:
            command = [binaries['control']['path'], '--' + mode]
        else:
            env.update(NEXAUI_PLATFORM='x11', NEXAUI_REQUIRE_VULKAN='1')
            command = [sys.executable, str(fixture), '--timeout', '120', '--work-dir', str(work/name),
                       '--', binaries['app']['path'], '--smoke']
        timeout = False
        with (work/(name+'.stdout')).open('wb') as out, (work/(name+'.stderr')).open('wb') as err:
            try:
                result = subprocess.run(command, env=env, stdout=out, stderr=err, timeout=180)
                code = result.returncode
            except subprocess.TimeoutExpired:
                code, timeout = None, True
        stdout = (work/(name+'.stdout')).read_text(errors='replace')
        stderr = (work/(name+'.stderr')).read_text(errors='replace')
        details = control_modules(stdout, mode == 'draw') if mode else {}
        layer_seen = 'libVkLayer_khronos_validation.so' in stderr or any(
            'libVkLayer_khronos_validation.so' in item['path'] for item in details.get('modules', []))
        layer_verified = (layer_seen == enabled) if libc or mode else None
        completed = (('Vulkan lifetime control complete: mode='+mode+
                      ' validation-errors=0 all-owned-objects-destroyed') in stdout) if mode else 'Vulkan frames' in stdout
        pixels = 'PIXELS\t256\t1024\tall-red-rgba-equal' in stdout if mode == 'draw' else None
        entry = dict(name=name, command=command, exit_code=code, timeout=timeout,
                     validation_library_observed=layer_seen, layer_control_verified=layer_verified,
                     smoke_completed=completed, all_pixels_verified=pixels,
                     leak_report='LeakSanitizer: detected memory leaks' in stderr, **details)
        report['runs'].append(entry)
        (work/'results.json').write_text(json.dumps(report, indent=2)+'\n')
        print(json.dumps({k: v for k, v in entry.items() if k != 'modules'}), flush=True)
        failed |= (code != 0 or layer_verified is False or not completed or pixels is False
                   or bool(details.get('identity_errors')) or entry['leak_report'])
    return 1 if failed else 0


if __name__ == '__main__':
    raise SystemExit(main())
