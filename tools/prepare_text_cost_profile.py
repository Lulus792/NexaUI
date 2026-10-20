"""Prepare an instrumented copy of the real text draw function for profiling.

No source or runtime settings are changed. The new output directory retains
original/copy hashes. Named stages are disjoint except raster work, explicitly
reported as nested within glyph-cache get. Run the normal library tests too.
"""
import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    group = parser.add_mutually_exclusive_group()
    group.add_argument('--baseline', type=Path, help='recorded text_layout.c with an identical private layout structure')
    group.add_argument('--baseline-commit', help='full 40-character commit hash containing src/text_layout.c')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    source = (root/'src/nexaui.c').read_text(encoding='utf-8')
    baseline = baseline_bytes = baseline_origin = None
    if args.baseline_commit:
        if not re.fullmatch('[0-9a-fA-F]{40}', args.baseline_commit):
            parser.error('--baseline-commit requires a full hexadecimal commit hash')
        baseline_origin = 'git:'+args.baseline_commit+':src/text_layout.c'
        baseline_bytes = subprocess.run(['git', 'show', args.baseline_commit+':src/text_layout.c'],
                                        cwd=root, check=True, stdout=subprocess.PIPE).stdout
    elif args.baseline:
        baseline_origin = str(args.baseline.resolve())
        baseline_bytes = args.baseline.read_bytes()
    if baseline_bytes is not None:
        baseline = baseline_bytes.decode('utf-8').replace('\r\n', '\n')
        current = (root/'src/text_layout.c').read_text(encoding='utf-8')
        pattern = r'struct nx_text_layout \{.*?\n\};'
        old_struct, new_struct = re.search(pattern, baseline, re.S), re.search(pattern, current, re.S)
        if not old_struct or not new_struct or old_struct[0] != new_struct[0]:
            raise RuntimeError('Baseline private layout ABI differs; direct paired rendering is unsupported')
    start = source.index('static nx_result nx_draw_text_layout_impl(')
    end = source.index('\nnx_result nx_draw_text_layout(', start)
    body = source[start:end]
    edits = [
        ('            result = nx_glyph_cache_create(', '            nx_cost_begin(0);\n            result = nx_glyph_cache_create('),
        ('                                           &caches[font_index]);', '                                           &caches[font_index]);\n            nx_cost_end(0);'),
        ('        result = nx_glyph_cache_get_fractional(', '        nx_cost_begin(1);\n        result = nx_glyph_cache_get_fractional('),
        ('                                    raster_ppem, &mask);', '                                    raster_ppem, &mask);\n        nx_cost_end(1);'),
        ('        size_t pixels_size = ', '        nx_cost_begin(2);\n        size_t pixels_size = '),
        ('        double units=coordinate_scale/raster_scale;', '        nx_cost_bytes(pixels_size);\n        nx_cost_end(2);\n        nx_cost_begin(3);\n        double units=coordinate_scale/raster_scale;'),
        ('origin,glyph.line_index,selection,selection_count):nx_draw_bitmap(ctx, dest, bitmap, glyph_tint);', 'origin,glyph.line_index,selection,selection_count):nx_draw_bitmap(ctx, dest, bitmap, glyph_tint);\n        nx_cost_end(3);'),
        ('    for (size_t i = 0; i < NX_FONT_COLLECTION_MAX_FONTS; ++i)', '    nx_cost_begin(4);\n    for (size_t i = 0; i < NX_FONT_COLLECTION_MAX_FONTS; ++i)'),
        ('        nx_glyph_cache_destroy(caches[i]);', '        nx_glyph_cache_destroy(caches[i]);\n    nx_cost_end(4);'),
    ]
    for old, new in edits:
        if body.count(old) != 1:
            raise RuntimeError('Production text implementation changed; review profiling boundary: '+old)
        body = body.replace(old, new)
    instrumented = '#include <stddef.h>\n#include "text_cost_hooks.h"\n'+source[:start]+body+source[end:]
    work = args.output.resolve()
    work.mkdir(parents=True, exist_ok=False)
    target = work/'text-cost-core.c'
    target.write_bytes(instrumented.encode('utf-8'))
    if baseline is not None:
        (work/'text-layout-baseline.c').write_bytes(baseline_bytes)
    report = dict(source=str(root/'src/nexaui.c'), output=str(target),
                  source_sha256=hashlib.sha256((root/'src/nexaui.c').read_bytes()).hexdigest(),
                  output_sha256=hashlib.sha256(target.read_bytes()).hexdigest(),
                  modifications='Only begin/end/byte accounting calls in nx_draw_text_layout_impl.',
                  stages=['cache-create', 'cache-get', 'mask-allocation-copy', 'placement-and-command', 'cache-destroy', 'raster-nested-in-cache-get'],
                  error_scope='Any failed draw aborts the benchmark; incomplete stage timing is not an accepted sample.')
    if baseline is not None:
        report['baseline'] = dict(source=baseline_origin, output=str(work/'text-layout-baseline.c'),
                                  source_sha256=hashlib.sha256(baseline_bytes).hexdigest(),
                                  output_sha256=hashlib.sha256((work/'text-layout-baseline.c').read_bytes()).hexdigest(),
                                  identical_private_layout_structure=True)
    (work/'manifest.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print(target)


if __name__ == '__main__':
    main()
