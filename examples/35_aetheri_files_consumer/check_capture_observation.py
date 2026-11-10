"""Check original-driver boundaries and independent read-only expectations.

This runs against the checked original export, without launching a compositor.
Actual native positive and wrong-expectation runs remain separate evidence.
"""
import ast
from pathlib import Path
import sys

from capture_observation import prepare


def main():
    if len(sys.argv) != 2:
        raise SystemExit('usage: check_capture_observation.py ORIGINAL_EXPORT')
    source = (Path(sys.argv[1]) / 'tools/test_files_view.py').read_text()
    output, changes = prepare(source)
    ast.parse(output)
    # Reverse only the three explicit insertions: every original assertion,
    # action, deadline and the capture delay must survive byte-for-byte.
    recovered = output
    for change in reversed(changes):
        assert recovered.count(change['after']) == 1
        recovered = recovered.replace(change['after'], change['before'])
    assert recovered == source
    assert output.count('time.sleep(.08)') == source.count('time.sleep(.08)')
    for change in changes:
        for invalid in (source.replace(change['before'], '', 1),
                        source + '\n' + change['before']):
            try:
                prepare(invalid)
            except SystemExit as error:
                assert str(error) == 'Changed original Files capture-observation boundary'
            else:
                raise AssertionError('Changed original boundary was accepted')
    print('CHECKED exact original recovery and six missing/duplicate boundary refusals')


if __name__ == '__main__':
    main()
