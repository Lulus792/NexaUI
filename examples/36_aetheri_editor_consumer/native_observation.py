"""Read the latest original frame without scanning all older frame records."""

BEFORE = r'''    def view():
        frames = re.findall(r'^FRAME ([^\n]+)$', log(), re.M)
        if not frames: return {}
        width, height, percent = frames[-1].split()[:3]
        return dict(width=width,height=height,percent=percent,**dict(re.findall(r'(\w+)=([^ ]+)', frames[-1])))
    def state(**values):
        return all(view().get(k) == str(v) for k, v in values.items())
'''

AFTER = r'''    def view():
        text = log(); limit = len(text)
        while True:
            marker = text.rfind('\nFRAME ', 0, limit)
            if marker < 0:
                if not text.startswith('FRAME ') or limit <= 6: return {}
                start = 6
            else: start = marker + 7
            end = text.find('\n', start, limit)
            if end < 0: end = limit
            if end > start:
                frame = text[start:end]; break
            if marker < 0: return {}
            limit = marker
        width, height, percent = frame.split()[:3]
        return dict(width=width,height=height,percent=percent,**dict(re.findall(r'(\w+)=([^ ]+)', frame)))
    def state(**values):
        if not values: return True
        observed = view()
        return all(observed.get(k) == str(v) for k, v in values.items())
'''


def prepare(source):
    if source.count(BEFORE) != 1:
        raise SystemExit('Changed original native frame-observation boundary')
    return source.replace(BEFORE, AFTER), dict(before=BEFORE, after=AFTER)
