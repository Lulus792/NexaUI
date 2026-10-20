"""Build a paired private Mesa cleanup candidate; never install a system driver.

Pinned unmodified source must reproduce the unload leak before applying the
two-file patch. Both ICD orders and validation states must then pass. The caller
must separately run NexaUI's complete native suite against the candidate.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import time
from urllib.request import Request, urlopen
import zipfile

ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT/'tools/mesa'
MESA_URL = 'https://archive.mesa3d.org/mesa-25.2.8.tar.xz'
MESA_SHA = '097842f3e49d996868b38688db87b006f7d4541e93ce86d2f341d8b3e7be7c93'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def acquire(url, digest, target, local=None):
    if local:
        shutil.copyfile(local, target)
    else:
        with urlopen(Request(url, headers={'User-Agent': 'NexaUI-CI/1.0'}), timeout=60) as response:
            with target.open('xb') as output:
                total = 0
                while block := response.read(1024*1024):
                    total += len(block)
                    if total > 64*1024*1024:
                        raise ValueError('source archive exceeds fixed bound')
                    output.write(block)
    if sha(target) != digest:
        raise ValueError('source archive SHA-256 mismatch: '+target.name)


def extract(archive, destination):
    destination.mkdir(exist_ok=False)
    with tarfile.open(archive) as source:
        source.extractall(destination, filter='data')
    children = list(destination.iterdir())
    if len(children) != 1 or not children[0].is_dir():
        raise ValueError('source archive must have one root directory')
    return children[0]


def patch(source):
    specification = json.loads((DATA/'25.2.8-lifetime.json').read_text())
    for name, hashes in specification['files'].items():
        if sha(source/name) != hashes['before_sha256']:
            raise ValueError('refusing unknown/modified Mesa source: '+name)
    result = subprocess.run(['patch', '-p1', '-i', str(DATA/'25.2.8-lifetime.patch')],
                            cwd=source, capture_output=True, text=True, check=True,
                            stdin=subprocess.DEVNULL, timeout=30)
    for name, hashes in specification['files'].items():
        if sha(source/name) != hashes['after_sha256']:
            raise ValueError('patched Mesa SHA-256 mismatch: '+name)
    return dict(files=specification['files'], patch_sha256=sha(DATA/'25.2.8-lifetime.patch'),
                stdout=result.stdout, stderr=result.stderr)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work-dir', type=Path, required=True)
    parser.add_argument('--control', type=Path, required=True)
    parser.add_argument('--before-results', type=Path, required=True)
    parser.add_argument('--after-results', type=Path, required=True)
    parser.add_argument('--github-env', type=Path)
    parser.add_argument('--archives', type=Path, help='optional directory of pinned offline inputs')
    args = parser.parse_args()
    if sys.platform != 'linux' or os.uname().machine != 'x86_64':
        parser.error('this paired private driver build requires Linux x86_64')
    control = args.control.resolve(strict=True)
    asahi = Path('/usr/share/vulkan/icd.d/asahi_icd.json')
    if not asahi.is_file():
        parser.error('the paired system Asahi manifest is missing')
    work = args.work_dir.resolve()
    work.mkdir(parents=True, exist_ok=False)
    receipts = work/'receipts'; receipts.mkdir()
    report = dict(scope='Private paired Mesa candidate, not an OS driver installation or SDK change.',
                  commands=[], source_url=MESA_URL, source_sha256=MESA_SHA, inputs=[])
    def save():
        (receipts/'build.json').write_text(json.dumps(report, indent=2)+'\n')
    def run(name, command, env=None, accepted=(0,)):
        entry = dict(name=name, command=[str(x) for x in command]); report['commands'].append(entry); save()
        start = time.monotonic()
        with (receipts/(name+'.stdout')).open('wb') as out, (receipts/(name+'.stderr')).open('wb') as err:
            try:
                result = subprocess.run(entry['command'], env=env, stdout=out, stderr=err, timeout=1200)
            except subprocess.TimeoutExpired:
                entry.update(exit_code=None, timed_out=True, seconds=time.monotonic()-start); save()
                raise
        entry.update(exit_code=result.returncode, seconds=time.monotonic()-start); save()
        print(name+': exit='+str(result.returncode), flush=True)
        if result.returncode not in accepted:
            raise RuntimeError(name+' failed; retained stdout/stderr in '+str(receipts))
        return result.returncode
    mesa_archive = work/'mesa-25.2.8.tar.xz'
    acquire(MESA_URL, MESA_SHA, mesa_archive,
            args.archives/mesa_archive.name if args.archives else None)
    source = extract(mesa_archive, work/'source')
    bundle = receipts/'corresponding-source'; bundle.mkdir()
    shutil.copy2(mesa_archive, bundle/mesa_archive.name)
    report['source_bundle'] = [dict(file=mesa_archive.name, sha256=sha(mesa_archive))]
    inputs = json.loads((DATA/'build-inputs.json').read_text())
    directories = {}
    for item in inputs:
        name = item['name']
        filename = item.get('filename', name+'.tar.gz' if name != 'meson' else 'meson.whl')
        archive = work/filename
        acquire(item['url'], item['sha256'], archive,
                args.archives/filename if args.archives else None)
        shutil.copy2(archive, bundle/filename)
        report['source_bundle'].append(dict(file=filename, sha256=sha(archive)))
        report['inputs'].append(item); save()
        if name == 'meson':
            destination = work/'meson-runtime'; destination.mkdir()
            with zipfile.ZipFile(archive) as wheel:
                for member in wheel.infolist():
                    if not (destination/member.filename).resolve().is_relative_to(destination):
                        raise ValueError('unsafe wheel member')
                    wheel.extract(member, destination)
            directories[name] = destination
        else:
            directories[name] = extract(archive, work/name)
    for filename in ('25.2.8-lifetime.patch', '25.2.8-lifetime.json', 'README.md', 'LICENSES.md'):
        shutil.copy2(DATA/filename, bundle/filename)
    save()
    prefix = work/'dependencies'
    run('spirv-configure', ['cmake', '-S', directories['SPIRV-Tools'], '-B', work/'spirv-build',
        '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_POSITION_INDEPENDENT_CODE=ON',
        '-DCMAKE_INSTALL_PREFIX='+str(prefix), '-DCMAKE_INSTALL_LIBDIR=lib',
        '-DSPIRV-Headers_SOURCE_DIR='+str(directories['SPIRV-Headers']),
        '-DSPIRV_SKIP_TESTS=ON', '-DSPIRV_SKIP_EXECUTABLES=ON'])
    run('spirv-build', ['cmake', '--build', work/'spirv-build', '--target', 'install', '--parallel', '4'])
    env = os.environ.copy()
    # Meson's generators must use the /usr/bin Python whose Mako/YAML packages
    # were installed on the isolated runner, even if another Python is on PATH.
    env['PATH'] = '/usr/lib/llvm-20/bin:/usr/bin:'+env.get('PATH', '')
    env['PYTHONPATH'] = str(directories['meson'])+os.pathsep+env.get('PYTHONPATH', '')
    env['PKG_CONFIG_PATH'] = os.pathsep.join([str(prefix/'lib/pkgconfig'),
        str(prefix/'share/pkgconfig'), env.get('PKG_CONFIG_PATH', '')])
    run('llvm-version', ['llvm-config', '--version'], env)
    meson = [sys.executable, '-m', 'mesonbuild.mesonmain']
    run('protocols-configure', [*meson, 'setup', work/'protocols-build',
        directories['wayland-protocols'], '--prefix='+str(prefix), '-Dtests=false'], env)
    run('protocols-install', [*meson, 'install', '-C', work/'protocols-build'], env)
    build = work/'build'
    run('mesa-configure', [*meson, 'setup', build, source, '--buildtype=debugoptimized',
        '--wrap-mode=nofallback', '-Db_ndebug=true', '-Dgallium-drivers=llvmpipe',
        '-Dvulkan-drivers=swrast', '-Dplatforms=x11,wayland', '-Dglx=disabled',
        '-Degl=disabled', '-Dgbm=disabled', '-Dopengl=false', '-Dgles1=disabled',
        '-Dgles2=disabled', '-Dgallium-va=disabled', '-Dgallium-vdpau=disabled',
        '-Dbuild-tests=false', '-Dtools=[]', '-Dllvm=enabled', '-Dshared-llvm=enabled'], env)
    target = 'src/gallium/targets/lavapipe/libvulkan_lvp.so'
    icd_target = 'src/gallium/targets/lavapipe/lvp_devenv_icd.x86_64.json'
    run('mesa-before-build', ['ninja', '-C', build, '-j4', target, icd_target], env)
    # Require the driver's existing SPIR-V tooling; do not drop internal validation.
    compile_commands = (build/'compile_commands.json').read_text()
    if '-DHAVE_SPIRV_TOOLS' not in compile_commands:
        raise RuntimeError('SPIRV-Tools was not enabled in the driver build')
    def snapshot(name):
        directory = receipts/name; directory.mkdir()
        library = directory/'libvulkan_lvp.so'; shutil.copy2(build/target, library)
        manifest = directory/'lvp_icd.json'
        metadata = json.loads((build/icd_target).read_text())
        metadata['ICD']['library_path'] = str(library)
        manifest.write_text(json.dumps(metadata, indent=2)+'\n')
        report[name] = dict(library=str(library), sha256=sha(library), bytes=library.stat().st_size,
                            manifest=str(manifest)); save()
        return manifest
    before = snapshot('before')
    run('before-orders', [sys.executable, ROOT/'tools/trace_vulkan_icd_order.py', '--control', control,
        '--first', asahi, '--second', before, '--work-dir', args.before_results.resolve()], accepted=(1,))
    baseline = json.loads((args.before_results/'results.json').read_text())
    for order in baseline['runs']:
        if len(order['controls']) != 4:
            raise RuntimeError('baseline controls incomplete')
        for case in order['controls']:
            if not case['smoke_completed'] or not case['layer_control_verified'] or case['identity_errors']:
                raise RuntimeError('baseline correctness/identity failed')
            if '-draw-' in case['name'] and not case['all_pixels_verified']:
                raise RuntimeError('baseline pixels differ')
            if order['name'] == 'first-second':
                if case['lvp_loaded_after_instance_destroy']:
                    raise RuntimeError('baseline did not unload Lavapipe')
                if '-draw-' in case['name'] and not (case['leak_report'] and case['exit_code'] == 1):
                    raise RuntimeError('baseline did not reproduce the draw leak')
                if case['exit_code'] != int(case['leak_report']):
                    raise RuntimeError('baseline device failed for an unexpected reason')
            elif case['exit_code'] or case['leak_report'] or not case['lvp_loaded_after_instance_destroy']:
                raise RuntimeError('baseline reverse order does not match the known retention control')
    original = receipts/'source-before'
    for name in json.loads((DATA/'25.2.8-lifetime.json').read_text())['files']:
        target_file = original/name; target_file.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source/name, target_file)
    report['patch'] = patch(source); save()
    run('component-checks', [sys.executable, ROOT/'tools/check_mesa_cleanup.py', '--source', source,
        '--before', original, '--after', source, '--work-dir', receipts/'components', '--require-dlclose'])
    run('mesa-after-build', ['ninja', '-C', build, '-j4', target, icd_target], env)
    after = snapshot('after')
    run('after-orders', [sys.executable, ROOT/'tools/trace_vulkan_icd_order.py', '--control', control,
        '--first', asahi, '--second', after, '--work-dir', args.after_results.resolve()])
    result = json.loads((args.after_results/'results.json').read_text())
    first = result['runs'][0]['controls']
    if len(first) != 4 or any(c['lvp_loaded_after_instance_destroy'] for c in first):
        raise RuntimeError('candidate success did not demonstrate real driver unload')
    report['paired_controls_passed'] = True; save()
    if args.github_env:
        with args.github_env.open('a') as output:
            output.write('NEXAUI_CI_LVP_MANIFEST='+str(after)+'\n')
    print('Candidate passed both fixed orders; complete native suite is still required.', flush=True)


if __name__ == '__main__':
    main()
