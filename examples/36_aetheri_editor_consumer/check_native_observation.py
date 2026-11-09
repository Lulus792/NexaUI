"""Differential full-history parsing and coherent state observation."""
import re
import textwrap
from native_observation import BEFORE, AFTER, prepare


def functions(block, read):
    namespace = dict(re=re, log=read)
    exec(compile(textwrap.dedent(block), '<original-frame-observation>', 'exec'), namespace)
    return namespace


def main():
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
    print('Original/latest frame parity:12 complete histories,60 state cases, old mixed-frame false positive rejected and three refused source drifts')


if __name__ == '__main__': main()
