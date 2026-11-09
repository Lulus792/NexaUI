"""Differential full-history parsing and coherent state observation."""
import re
import textwrap
import os
import tempfile
from pathlib import Path
from unittest.mock import patch
from native_observation import BEFORE, AFTER, prepare, CAPTURE_BEFORE, CAPTURE_AFTER, prepare_focus_capture, LOG_BEFORE, LOG_AFTER, prepare_log_cache


def functions(block, read):
    namespace = dict(re=re, log=read)
    exec(compile(textwrap.dedent(block), '<original-frame-observation>', 'exec'), namespace)
    return namespace


def check_log_cache(block=LOG_AFTER):
    with tempfile.TemporaryDirectory(prefix='nexaui-observation-') as directory:
        root = Path(directory)
        def reader(source):
            namespace = dict(root=root, current_name='app')
            exec(compile(textwrap.dedent(source), '<original-full-log-reader>', 'exec'), namespace)
            return namespace
        before, after = reader(LOG_BEFORE), reader(block)
        checks = 0
        def same(name=None):
            nonlocal checks
            assert before['log'](name) == after['log'](name)
            checks += 1
        path = root/'app.log'
        same(); path.write_bytes(b''); same()
        path.write_bytes(b'FRAME 100 200 100 docs=1\r\n'); same()
        with path.open('ab') as stream: stream.write('é🌍\nFRAME 100 200 100 docs=2'.encode())
        same(); same()
        with path.open('ab') as stream: stream.write(b'\npartial \xc3')
        same()
        with path.open('ab') as stream: stream.write(b'\xa9\xff\r\n')
        same()
        path.write_bytes(b'truncated\n'); same()
        stamp = path.stat(); path.write_bytes(b'overwrote\n')
        os.utime(path, ns=(stamp.st_atime_ns, stamp.st_mtime_ns+2000000000)); same()
        replacement = root/'replacement'; replacement.write_bytes(b'new identity\n'); replacement.replace(path); same()
        (root/'server.log').write_bytes(b'CAPTURE 1 0\n'); same('server'); same()
        before['current_name'] = after['current_name'] = 'server'; same(); same('app')
        path.unlink(); same('app'); path.write_bytes(b'recreated\n'); same('app')
        before['current_name'] = after['current_name'] = 'app'
        # A write between metadata observation and return cannot make a cached
        # old text permanent. The next observation must see the appended frame.
        real_read = Path.read_text
        raced = reader(block); initial = real_read(path, errors='replace')
        def racing_read(p, *args, **kwargs):
            text = real_read(p, *args, **kwargs)
            with p.open('ab') as stream: stream.write(b'FRAME 100 200 100 docs=3\n')
            return text
        with patch.object(Path, 'read_text', racing_read): assert raced['log']() == initial
        assert raced['log']() == before['log'](); checks += 2
        # Count real full-text reads, not elapsed time. The old implementation
        # is the behavioral counterexample for this unchanged-file workload.
        counts = []
        for source in (LOG_BEFORE, block):
            candidate = reader(source); reads = [0]
            def counted(p, *args, **kwargs):
                reads[0] += 1
                return real_read(p, *args, **kwargs)
            expected = real_read(path, errors='replace')
            with patch.object(Path, 'read_text', counted):
                for _ in range(64): assert candidate['log']() == expected
            counts.append(reads[0])
        assert counts == [64, 1], ('unchanged-file full reads', counts)
        print('Full-log parity:', checks, 'real-file observations; unchanged-file reads64→1; racing append retained')


def main():
    check_log_cache()
    try: check_log_cache(LOG_BEFORE)
    except AssertionError as error: assert error.args[0] == ('unchanged-file full reads', [64, 64])
    else: raise AssertionError('old full-reader work counterexample passed')
    adapted, change = prepare_log_cache('head\n'+LOG_BEFORE+'tail\n')
    assert adapted == 'head\n'+LOG_AFTER+'tail\n' and change == dict(before=LOG_BEFORE, after=LOG_AFTER)
    for changed in ('', LOG_BEFORE+LOG_BEFORE, LOG_BEFORE.replace("errors='replace'", "errors='ignore'")):
        try: prepare_log_cache(changed)
        except SystemExit: pass
        else: raise AssertionError('changed original log reader accepted')
    current = ['']; reads = [0]
    def read():
        reads[0] += 1
        return current[0]
    before, after = functions(BEFORE, read), functions(AFTER, read)
    frame = 'FRAME 1100 740 100 docs=1 active=7 revision=10 cursor=0 selected=0\n'
    older = ''.join('FRAME 320 240 200 docs=2 revision='+str(i)+'\ntrace é🌍\n' for i in range(10000))
    cases = ['', 'trace\n', 'FRAME ', 'FRAME \n', frame, frame.rstrip('\n'),
             'trace\n'+frame+'after\n', frame+'FRAME \n', frame+'FRAME ',
             older+frame, older+frame+'FRAME \nFRAME ',
             'FRAME 320 240 200 docs=3\n'+frame+'partial trace']
    for text in cases:
        current[0] = text
        assert before['view']() == after['view']()
        for expected in ({}, dict(docs=1), dict(docs=1, cursor=0, selected=0), dict(revision=9), dict(missing=0)):
            assert before['state'](**expected) == after['state'](**expected)
    current[0] = older+frame; reads[0] = 0
    assert after['state'](docs=1, cursor=0, selected=0) and reads[0] == 1
    assert after['state']() and reads[0] == 1
    # Two different frames must not fabricate a state present in neither one.
    first = 'FRAME 1100 740 100 docs=1 cursor=99\n'
    second = 'FRAME 1100 740 100 docs=0 cursor=0\n'
    samples = iter((first, second))
    assert functions(BEFORE, lambda: next(samples))['state'](docs=1, cursor=0)
    samples = iter((first, second))
    assert not functions(AFTER, lambda: next(samples))['state'](docs=1, cursor=0)
    adapted, change = prepare('head\n'+BEFORE+'tail\n')
    assert adapted == 'head\n'+AFTER+'tail\n' and change == dict(before=BEFORE, after=AFTER)
    for changed in ('', BEFORE+BEFORE, BEFORE.replace('frames[-1]', 'frames[0]')):
        try: prepare(changed)
        except SystemExit: pass
        else: raise AssertionError('changed original boundary accepted')
    adapted, change = prepare_focus_capture('head\n'+CAPTURE_BEFORE+'\ntail\n')
    assert adapted == 'head\n'+CAPTURE_AFTER+'\ntail\n' and change == dict(before=CAPTURE_BEFORE, after=CAPTURE_AFTER)
    for changed in ('', CAPTURE_BEFORE+CAPTURE_BEFORE, CAPTURE_BEFORE.replace('.08', '.8')):
        try: prepare_focus_capture(changed)
        except SystemExit: pass
        else: raise AssertionError('changed original capture boundary accepted')
    # The predicate must refuse old diagnostics, a pending focus paint and an
    # older settled frame followed by a pending frame. Only the latest frame
    # can establish that the additional work completed.
    for pending, expected in ((None, False), (1, False), (0, True)):
        current[0] = frame if pending is None else frame.rstrip('\n')+' focus_pending='+str(pending)+'\n'
        assert after['state'](focus_pending=0) == expected
    current[0] = frame.rstrip('\n')+' focus_pending=0\n'+frame.rstrip('\n')+' focus_pending=1\n'
    assert not after['state'](focus_pending=0)
    print('Original/latest frame parity:12 complete histories,60 state cases, mixed-frame false positive rejected; focus capture accepts only completed latest paint, nine source drifts refused')


if __name__ == '__main__': main()
