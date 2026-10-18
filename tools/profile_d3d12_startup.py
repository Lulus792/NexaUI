#!/usr/bin/env python3
"""Run a fixed ABBA sequence of native startup diagnostics on Windows.

Each process opens its own brief nonactivating window, presents, waits for DWM,
sends input only to its HWND and acknowledges a new composed file-page frame.
The parent measures launch through the READY marker, including initialization.
No driver-cache controls, cache purge, preference changes or warm-up exclusions.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import statistics
import subprocess
import sys
import threading
import time


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(executable, font, adapter, directory):
    directory.mkdir()
    environment = dict(os.environ)
    if adapter == 'warp':
        environment['NEXAUI_TEST_FORCE_WARP'] = '1'
    else:
        environment.pop('NEXAUI_TEST_FORCE_WARP', None)
    command = [str(executable), str(font)]
    lines, markers, reader_errors = [], [], []
    with (directory/'stderr.log').open('w', encoding='utf-8') as errors:
        started = time.perf_counter_ns()
        child = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=errors,
                                 text=True, encoding='utf-8', env=environment)
        def read():
            try:
                for line in child.stdout:
                    if line.startswith('READY ') and not markers:
                        markers.append(time.perf_counter_ns())
                    lines.append(line)
            except Exception as error:
                reader_errors.append(repr(error))
        reader = threading.Thread(target=read, daemon=True)
        reader.start()
        timed_out = False
        try:
            child.wait(timeout=60)
        except subprocess.TimeoutExpired:
            timed_out = True
            child.kill()  # Only this fixture's exact process handle.
            child.wait()
        reader.join(timeout=5)
    raw = ''.join(lines)
    (directory/'stdout.log').write_text(raw, encoding='utf-8')
    ready = next((line[6:] for line in lines if line.startswith('READY ')), None)
    result = dict(command=command, adapter_requested=adapter, exit_code=child.returncode,
                  timed_out=timed_out, stdout_complete=not reader.is_alive() and not reader_errors,
                  reader_errors=reader_errors, binary_sha256=digest(executable),
                  launch_to_marker_ms=(markers[0]-started)/1e6 if markers else None,
                  ready=json.loads(ready) if ready else None,
                  final=json.loads(raw.split('FINAL ', 1)[1]) if 'FINAL ' in raw else None)
    (directory/'result.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    if (child.returncode or timed_out or not result['stdout_complete'] or not result['ready']
            or result['ready'].get('schema') != 1 or not result['final']):
        raise RuntimeError('Incomplete native startup task; evidence retained: '+str(directory))
    if bool(result['ready']['warp']) != (adapter == 'warp'):
        raise RuntimeError('Requested hardware unavailable; fallback is not a hardware timing result: '+str(directory))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--before', required=True, type=Path)
    parser.add_argument('--after', required=True, type=Path)
    parser.add_argument('--font', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path, help='must not exist')
    parser.add_argument('--blocks', type=int, default=2, help='1..10 complete ABBA blocks per adapter; default 2')
    parser.add_argument('--adapter', choices=['hardware', 'warp', 'both'], default='both')
    args = parser.parse_args()
    if sys.platform != 'win32':
        parser.error('Native Windows execution is required')
    if not 1 <= args.blocks <= 10:
        parser.error('--blocks must be 1..10')
    args.before, args.after, args.font, args.output = (path.resolve() for path in (args.before, args.after, args.font, args.output))
    if args.output.exists():
        parser.error('--output must not exist; previous evidence is preserved')
    for path in (args.before, args.after, args.font):
        if not path.is_file():
            parser.error('Missing input: '+str(path))
    args.output.mkdir(parents=True)
    adapters = ['hardware', 'warp'] if args.adapter == 'both' else [args.adapter]
    plan = [dict(adapter=adapter, index=index+1, variant=variant)
            for adapter in adapters for index, variant in enumerate(['before', 'after', 'after', 'before']*args.blocks)]
    report = dict(status='failed', font_sha256=digest(args.font), binaries={
                      'before': dict(path=str(args.before), sha256=digest(args.before)),
                      'after': dict(path=str(args.after), sha256=digest(args.after))},
                  plan=plan, observations=[], summary=[],
                  scope='Launch-to-composed owned-HWND input acknowledgement; includes all preparation. No physical scanout, human interaction or universal cold-cache claim.')
    # Persist the order before starting any child, and update after every run.
    def save():
        (args.output/'report.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    save()
    try:
        for item in plan:
            name=f"{item['adapter']}-{item['index']:02d}-{item['variant']}"
            result=run(getattr(args, item['variant']), args.font, item['adapter'], args.output/name)
            report['observations'].append(dict(**item, result=result))
            save()
            print(name, round(result['launch_to_marker_ms'], 3), 'ms', flush=True)
        for adapter in adapters:
            for variant in ('before', 'after'):
                records=[row['result'] for row in report['observations'] if row['adapter']==adapter and row['variant']==variant]
                values=[record['launch_to_marker_ms'] for record in records]
                report['summary'].append(dict(adapter=adapter, variant=variant, n=len(values),
                    launch_ms=dict(min=min(values), median=statistics.median(values), max=max(values)),
                    median_first_render_ms=statistics.median(record['ready']['first_render_ms'] for record in records)))
        report['status']='passed'
    finally:
        save()
    print(json.dumps(report['summary'], indent=2))


if __name__ == '__main__':
    main()
