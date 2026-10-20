"""Attribute Vulkan sanitizer reports with fixed explicit-validation on/off runs.

Run under xvfb-run. Uses the normal native-clear smoke executable and the private
Weston fixture. Leak detection remains enabled. On glibc, LD_DEBUG=files retains
DSO load addresses even if the module has unloaded before LeakSanitizer reports.
This is diagnostic evidence, not a leak suppression or a production policy.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--app', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    if sys.platform != 'linux':
        parser.error('this diagnostic requires Linux and a private X11 display')
    app, work = args.app.resolve(), args.work_dir.resolve()
    if not app.is_file():
        parser.error('native smoke executable is missing')
    work.mkdir(parents=True, exist_ok=False)
    try:
        libc = os.confstr('CS_GNU_LIBC_VERSION')
    except (ValueError, OSError):
        libc = None
    report = dict(scope='Two fixed native Vulkan smoke runs; no leak suppression. Full CTest remains a separate gate.',
                  app=str(app), app_sha256=hashlib.sha256(app.read_bytes()).hexdigest(),
                  libc=libc, module_logging_supported=bool(libc), runs=[])
    fixture = Path(__file__).resolve().parents[1]/'tests/run_weston_native.py'
    failed = False
    for name, enabled in (('explicit-validation', True), ('without-explicit-validation', False)):
        settings = ['env', '-u', 'VK_INSTANCE_LAYERS', '-u', 'VK_LAYER_VALIDATE_SYNC',
                    'NEXAUI_PLATFORM=x11', 'NEXAUI_REQUIRE_VULKAN=1', 'LD_DEBUG=files',
                    'ASAN_OPTIONS=halt_on_error=1:detect_leaks=1:fast_unwind_on_malloc='+('0' if libc else '1'),
                    'LSAN_OPTIONS=report_objects=1']
        if enabled:
            settings += ['VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation', 'VK_LAYER_VALIDATE_SYNC=1']
        command = [sys.executable, str(fixture), '--timeout', '120', '--work-dir', str(work/name), '--',
                   *settings, str(app), '--smoke']
        with (work/(name+'.stdout')).open('wb') as out, (work/(name+'.stderr')).open('wb') as err:
            result = subprocess.run(command, stdout=out, stderr=err, timeout=180)
        stdout = (work/(name+'.stdout')).read_text(errors='replace')
        stderr = (work/(name+'.stderr')).read_text(errors='replace')
        layer_seen = 'libVkLayer_khronos_validation.so' in stderr
        valid_control = not libc or layer_seen == enabled
        entry = dict(name=name, command=command, exit_code=result.returncode,
                     validation_library_in_loader_log=layer_seen,
                     layer_control_verified=valid_control if libc else None,
                     smoke_completed='Vulkan frames' in stdout,
                     leak_report='LeakSanitizer: detected memory leaks' in stderr)
        report['runs'].append(entry)
        (work/'results.json').write_text(json.dumps(report, indent=2)+'\n')
        print(json.dumps(entry), flush=True)
        failed |= result.returncode != 0 or not valid_control or not entry['smoke_completed']
    return 1 if failed else 0


if __name__ == '__main__':
    raise SystemExit(main())
