#!/usr/bin/env python3
"""Export a recorded NexaUI paint/core implementation for paired diagnostics."""
import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--revision', required=True)
    parser.add_argument('--output-dir', required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    destination = Path(args.output_dir).resolve()
    if not destination.is_relative_to(root / 'out') or destination.exists():
        parser.error('output must be a new directory inside the repository out/ directory')

    def git(*arguments):
        return subprocess.check_output(['git', '-C', str(root), *arguments])

    revision = git('rev-parse', '--verify', args.revision + '^{commit}').decode().strip()
    files = {name: git('show', revision + ':src/' + name) for name in ('paint.c', 'nexaui.c')}
    source = files['nexaui.c'].decode('utf-8')
    names = set()
    pattern = r'^(?!static\b)(?:const[ \t]+)?[A-Za-z_]\w*(?:[ \t]+|[ \t]*\*+[ \t]*)(nx_\w+)[ \t]*\('
    for match in re.finditer(pattern, source, re.MULTILINE):
        cursor, depth = match.end(), 1
        while cursor < len(source) and depth:
            depth += (source[cursor] == '(') - (source[cursor] == ')')
            cursor += 1
        if source[cursor:].lstrip().startswith('{'):
            names.add(match.group(1))
    required = {'nx_context_create', 'nx_context_destroy', 'nx_begin_frame',
                'nx_end_frame', 'nx_frame_commands', 'nx_plot_multi'}
    if not required <= names:
        parser.error('baseline lacks required public plot/context definitions')
    prefix = '/* Generated baseline-only symbol isolation. */\n'
    prefix += ''.join(f'#define {name} nx_before_core_{name[3:]}\n' for name in sorted(names))
    files['core_prefix.h'] = prefix.encode()
    destination.mkdir(parents=True)
    for name, data in files.items():
        (destination / name).write_bytes(data)
    manifest = {'revision': revision, 'core_symbols': len(names),
                'files': {name: hashlib.sha256(data).hexdigest() for name, data in files.items()}}
    (destination / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(json.dumps(manifest, indent=2))


if __name__ == '__main__':
    main()
