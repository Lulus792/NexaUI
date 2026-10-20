"""Test pinned Mesa source ownership, including real executable mappings/finalizers."""
import argparse
import json
import os
from pathlib import Path
import resource
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True, help='full pinned source/header tree')
    parser.add_argument('--before', type=Path, required=True, help='original two-file source tree')
    parser.add_argument('--after', type=Path, required=True, help='patched two-file source tree')
    parser.add_argument('--work-dir', type=Path, required=True)
    parser.add_argument('--require-dlclose', action='store_true')
    args = parser.parse_args()
    if sys.platform != 'linux':
        parser.error('native Linux component tests are required')
    source, before, after = (p.resolve() for p in (args.source, args.before, args.after))
    work = args.work_dir.resolve(); work.mkdir(parents=True, exist_ok=False)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    flags = ['-std=c17', '-g', '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
        '-Wall', '-Wextra', '-Wpedantic', '-D_GNU_SOURCE', '-DHAVE_PTHREAD', '-DHAVE_THRD_CREATE',
        '-DHAVE_STRUCT_TIMESPEC', '-DHAVE_SECURE_GETENV', '-DHAVE_FUNC_ATTRIBUTE_UNUSED',
        '-DHAVE_LINUX_FUTEX_H', '-DUTIL_ARCH_LITTLE_ENDIAN=1', '-DUTIL_ARCH_BIG_ENDIAN=0', '-DMESA_DEBUG=0',
        '-I'+str(source/'src'), '-I'+str(source/'include'), '-I'+str(source/'src/util'),
        '-I'+str(source/'src/gallium/auxiliary/rtasm')]
    env = os.environ.copy()
    env.update(ASAN_OPTIONS='halt_on_error=1:detect_leaks=1', UBSAN_OPTIONS='halt_on_error=1')
    report = dict(scope='Actual source component tests, separate from full Vulkan/native suite.', commands=[])
    def run(name, command, accepted=(0,)):
        result = subprocess.run([str(x) for x in command], env=env, capture_output=True, timeout=120)
        (work/(name+'.stdout')).write_bytes(result.stdout); (work/(name+'.stderr')).write_bytes(result.stderr)
        report['commands'].append(dict(name=name, command=[str(x) for x in command], exit_code=result.returncode))
        (work/'results.json').write_text(json.dumps(report, indent=2)+'\n')
        if result.returncode not in accepted:
            raise RuntimeError(name+' failed; retained output in '+str(work))
        return result
    close = work/'cpu-close'
    run('cpu-close-build', ['clang', *flags, ROOT/'tests/mesa_cpu_dlclose.c', '-ldl', '-o', close])
    for name, tree in (('before', before), ('after', after)):
        rtasm = work/('rtasm-'+name)
        run('rtasm-'+name+'-build', ['clang', *flags,
            '-DNEXAUI_MESA_MM_SOURCE="'+str(source/'src/util/u_mm.c')+'"',
            '-DNEXAUI_MESA_RTASM_SOURCE="'+str(tree/'src/gallium/auxiliary/rtasm/rtasm_execmem.c')+'"',
            ROOT/'tests/mesa_rtasm_lifetime.c', source/'src/util/futex.c', '-pthread', '-lm', '-o', rtasm])
        result = run('rtasm-'+name, [rtasm], accepted=(-6,) if name == 'before' else (0,))
        if name == 'before' and b'live_allocations == 0 && live_maps == 0' not in result.stderr:
            raise RuntimeError('original rtasm failed for a reason other than retained pool ownership')
        fixture = work/('cpu-'+name+'.so')
        run('cpu-'+name+'-build', ['clang', *flags, '-fPIC', '-shared', '-fvisibility=hidden',
            '-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections',
            '-DNEXAUI_MESA_CPU_SOURCE="'+str(tree/'src/util/u_cpu_detect.c')+'"',
            ROOT/'tests/mesa_cpu_lifetime.c', '-pthread', '-lm', '-o', fixture])
        result = run('cpu-'+name+'-exit', [close, fixture, '--exit-scope'])
        events = [int(line.split()[1]) for line in result.stdout.decode().splitlines() if line.startswith('CPU_EVENT ')]
        expected = [1, 3] if name == 'before' else [1, 3, 2]
        if events != expected:
            raise RuntimeError('CPU process-exit finalizer order differs: '+repr(events))
        accepted = ((1,) if name == 'before' else (0,)) if args.require_dlclose else (0, 1, 77)
        result = run('cpu-'+name+'-dlclose', [close, fixture], accepted=accepted)
        if result.returncode == 77:
            report['dlclose_exercised'] = False
        else:
            report['dlclose_exercised'] = True
            expected = b'CPU_CLOSE shutdown=1 default_fini=1 freed='+ (b'0' if name == 'before' else b'1')
            if expected not in result.stdout:
                raise RuntimeError('CPU dlclose lifetime evidence differs')
    report['component_checks_passed'] = True
    (work/'results.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(dict(component_checks_passed=True, dlclose_exercised=report['dlclose_exercised'])))


if __name__ == '__main__':
    main()
