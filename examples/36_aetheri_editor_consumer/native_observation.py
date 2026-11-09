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


CAPTURE_BEFORE = "        time.sleep(.08); sequence = len(images)//3+1; server.send_signal(signal.SIGUSR1)"
CAPTURE_AFTER = """        time.sleep(.08)
        wait(lambda: state(focus_pending=0), label+' completed focus paint')
        sequence = len(images)//3+1; server.send_signal(signal.SIGUSR1)"""


def prepare_focus_capture(source):
    if source.count(CAPTURE_BEFORE) != 1:
        raise SystemExit('Changed original native capture boundary')
    return source.replace(CAPTURE_BEFORE, CAPTURE_AFTER), dict(before=CAPTURE_BEFORE, after=CAPTURE_AFTER)


LOG_BEFORE = '''    def log(name=None):
        if name is None: name = current_name
        path = root / (name + '.log')
        return path.read_text(errors='replace') if path.exists() else ''
'''

# The original driver owns these regular append-only log streams. Keep the
# complete decoded text, not a tail or a filtered view. A changed identity,
# size or timestamp forces the original full read/UTF-8 replacement behavior.
# Record the pre-read stamp: an append racing the read must invalidate the
# cached snapshot on the next observation. No file descriptor stays open.
LOG_AFTER = '''    log_snapshots = {}
    def log(name=None):
        if name is None: name = current_name
        path = root / (name + '.log')
        try: info = path.stat()
        except FileNotFoundError:
            log_snapshots.pop(path, None)
            return ''
        stamp = (info.st_dev, info.st_ino, info.st_size, info.st_mtime_ns, info.st_ctime_ns)
        previous = log_snapshots.get(path)
        if previous is not None and previous[0] == stamp: return previous[1]
        observed = path.read_text(errors='replace')
        log_snapshots[path] = (stamp, observed)
        return observed
'''


def prepare_log_cache(source):
    if source.count(LOG_BEFORE) != 1:
        raise SystemExit('Changed original native log-reader boundary')
    return source.replace(LOG_BEFORE, LOG_AFTER), dict(before=LOG_BEFORE, after=LOG_AFTER)
