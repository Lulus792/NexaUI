"""App-owned Chromium source for the example's private native browser case."""

import base64
import json
import os
from pathlib import Path
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import time


CDP_LIMIT = 8 * 1024 * 1024
PNG_LIMIT = 4 * 1024 * 1024


class Chromium:
    def __init__(self, engine, profile):
        self.buffer = bytearray()
        self.serial = 0
        self.session = None
        self.target = None
        self.socket, child = socket.socketpair()
        try:
            with (profile / 'chromium.stderr').open('wb') as log:
                self.process = subprocess.Popen(
                    [sys.executable, __file__, '--launch', str(child.fileno()), engine,
                     f'--user-data-dir={profile}/data', '--remote-debugging-pipe',
                     '--disable-background-networking', '--disable-gpu', '--no-first-run'],
                    stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=log,
                    pass_fds=(child.fileno(),), start_new_session=True)
        except BaseException:
            self.socket.close()
            raise
        finally:
            child.close()

    def command(self, method, params=None, *, browser=False, timeout=4):
        self.serial += 1
        command = {'id': self.serial, 'method': method}
        if params is not None:
            command['params'] = params
        if self.session and not browser:
            command['sessionId'] = self.session
        deadline = time.monotonic() + timeout
        self.socket.settimeout(timeout)
        self.socket.sendall(json.dumps(command, separators=(',', ':')).encode() + b'\0')
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError(method)
            end = self.buffer.find(0)
            if end < 0:
                if len(self.buffer) == CDP_LIMIT:
                    raise ValueError('Oversized Chromium response')
                self.socket.settimeout(remaining)
                chunk = self.socket.recv(min(65536, CDP_LIMIT - len(self.buffer)))
                if not chunk:
                    raise EOFError(method)
                self.buffer.extend(chunk)
                continue
            packet = json.loads(self.buffer[:end])
            del self.buffer[:end + 1]
            if packet.get('id') == self.serial:
                if 'error' in packet:
                    raise RuntimeError(f'{method}: {packet["error"]["message"]}')
                return packet['result']

    def open_target(self):
        if self.target:
            self.command('Target.closeTarget', {'targetId': self.target}, browser=True)
        self.target = self.command('Target.createTarget', {'url': 'about:blank'}, browser=True)['targetId']
        self.session = self.command('Target.attachToTarget', {'targetId': self.target, 'flatten': True},
                                    browser=True)['sessionId']
        self.command('Network.enable')
        self.command('Network.emulateNetworkConditions', {'offline': True, 'latency': 0,
                         'downloadThroughput': 0, 'uploadThroughput': 0})

    def capture(self, width, height, kind):
        scale = 2 if kind == 4 else 1
        css_width, css_height = width // scale, height // scale
        self.command('Emulation.setDeviceMetricsOverride', {'width': css_width, 'height': css_height,
                     'deviceScaleFactor': scale, 'mobile': False})
        color = {0: '#d34f1f', 1: '#1753c5', 3: '#1753c5', 4: '#bf3f89'}[kind]
        patch = '<i></i>' if kind == 3 else ''
        html = ('<!doctype html><meta charset=utf-8><title>Content test</title>'
                '<style>html,body{margin:0;width:100%;height:100%;overflow:hidden;background:' + color +
                '} main{width:100%;height:100%} i{position:absolute;left:50%;top:50%;width:25%;height:25%;'
                'background:#47a31f}</style><main aria-label="Engine fixture">' + patch + '</main>')
        frame = self.command('Page.getFrameTree')['frameTree']['frame']['id']
        self.command('Page.setDocumentContent', {'frameId': frame, 'html': html})
        return self.screenshot(width,height,scale)

    def value(self, expression):
        result = self.command('Runtime.evaluate', {'expression': expression, 'returnByValue': True})
        if 'exceptionDetails' in result:
            raise RuntimeError('Browser fixture expression failed')
        return result['result']['value']

    def screenshot(self, width, height, scale):
        css_width, css_height = width // scale, height // scale
        metrics = self.value('[innerWidth,innerHeight,devicePixelRatio]')
        if metrics != [css_width, css_height, scale]:
            raise ValueError('Chromium viewport metrics differ from the request')
        encoded = self.command('Page.captureScreenshot', {'format': 'png', 'captureBeyondViewport': False})['data']
        png = base64.b64decode(encoded, validate=True)
        if (len(png) > PNG_LIMIT or not png.startswith(b'\x89PNG\r\n\x1a\n') or len(png) < 24 or
                struct.unpack('>II', png[16:24]) != (width, height)):
            raise ValueError('Chromium physical frame differs from the request')
        return png, metrics

    def edit(self, kind, raw, begin, end):
        if kind == 5:
            if self.value("!!document.querySelector('#editor')"):
                self.command('Input.imeSetComposition', {'text': '', 'selectionStart': 0, 'selectionEnd': 0})
            self.value("""(() => {
                let editor = document.querySelector('#editor');
                if (!editor) {
                    editor = document.createElement('div');
                    editor.id = 'editor';
                    editor.contentEditable = 'true';
                    editor.setAttribute('aria-label', 'Fixture text');
                    editor.style.cssText = 'position:absolute;left:48px;top:36px;width:260px;' +
                        'padding:4px;color:#111;background:white;font:24px monospace;min-height:1em';
                    document.body.append(editor);
                    editor.dataset.committed = '';
                    editor.dataset.preedit = '';
                    editor.addEventListener('compositionupdate', e => editor.dataset.preedit = e.data);
                    editor.addEventListener('compositionend', () => editor.dataset.preedit = '');
                }
                editor.focus();
                return true;
            })()""")
        elif kind == 6:
            text = raw.decode('utf-8')
            self.value("""(() => {
                const editor = document.querySelector('#editor');
                if (!editor.dataset.preedit) editor.dataset.committed = editor.textContent;
                return true;
            })()""")
            def offset(index):
                if index == -1:
                    index = len(raw)
                if not 0 <= index <= len(raw):
                    raise ValueError('Invalid preedit offset')
                return len(raw[:index].decode('utf-8').encode('utf-16-le')) // 2
            first,last=sorted((offset(begin),offset(end)))
            self.command('Input.imeSetComposition', {'text': text, 'selectionStart': first, 'selectionEnd': last})
        elif kind == 7:
            self.command('Input.insertText', {'text': raw.decode('utf-8')})
        elif kind == 8:
            self.command('Input.imeSetComposition', {'text': '', 'selectionStart': 0, 'selectionEnd': 0})
            self.value("document.querySelector('#editor').blur(); true")
        state = self.value("""(() => {
            const editor = document.querySelector('#editor');
            const focused = document.activeElement === editor;
            let caret = [0,0,0,0];
            if (focused) {
                const selection = getSelection();
                let box = editor.getBoundingClientRect();
                if (selection.rangeCount) {
                    const range = selection.getRangeAt(0).cloneRange();
                    range.collapse(false);
                    const cursor = range.getBoundingClientRect();
                    if (cursor.height) box = cursor;
                    else box = {x:box.x+4, y:box.y+4, height:24};
                }
                caret = [Math.floor(box.x), Math.floor(box.y), 1, Math.ceil(box.height)];
            }
            const committed = editor.dataset.preedit ? editor.dataset.committed : editor.textContent;
            return [focused, editor.dataset.preedit.length, committed.length, committed === 'aü', caret];
        })()""")
        focused, preedit, committed, expected, caret = state
        if focused != (kind != 8):
            raise RuntimeError('Chromium focus disagrees with the requested edit')
        return struct.pack('>4H', *caret), (int(focused), preedit, committed, int(expected))

    def crash(self):
        os.killpg(self.process.pid, signal.SIGKILL)
        result = self.process.wait(timeout=3)
        self.socket.close()
        if result != -signal.SIGKILL:
            raise RuntimeError(f'Expected browser to exit on SIGKILL, got {result}')
        return result

    def close(self):
        try:
            if self.process.poll() is None:
                try:
                    self.command('Browser.close', browser=True, timeout=1)
                except (OSError, EOFError, RuntimeError):
                    pass
                try:
                    self.process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    os.killpg(self.process.pid, signal.SIGKILL)
                    self.process.wait(timeout=3)
            return self.process.returncode
        finally:
            self.socket.close()


def serve(engine, socket_path):
    def interrupt(*_):
        raise KeyboardInterrupt

    signal.signal(signal.SIGTERM, interrupt)
    with tempfile.TemporaryDirectory(prefix='chromium-profile-', dir=socket_path.parent) as temp:
        profile = Path(temp)
        browser = Chromium(engine, profile)
        try:
            version = browser.command('Browser.getVersion', browser=True)['product']
            generation = 0
            with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as listener:
                listener.bind(str(socket_path))
                listener.listen(1)
                print(f'BROWSER_ENGINE {version}', flush=True)
                while True:
                    peer, _ = listener.accept()
                    with peer:
                        peer.settimeout(1)
                        with peer.makefile('rb') as request:
                            line = request.readline(128)
                            if not line.endswith(b'\n'):
                                raise ValueError('Invalid browser request framing')
                            fields = list(map(int, line.split()))
                            tab, resize, width, height, kind = fields[:5]
                            raw = b''
                            if kind >= 5:
                                begin,end,size = fields[5:]
                                if not 0 <= size <= 256:
                                    raise ValueError('Browser edit exceeds fixture limit')
                                raw = request.read(size)
                                if len(raw) != size:
                                    raise ValueError('Truncated browser edit')
                            if request.read(1):
                                raise ValueError('Unexpected browser request bytes')
                        if not (0 < tab < 2**64 and 0 < resize < 2**64 and 0 < width <= 2048 and
                                0 < height <= 2048 and width*height*4 <= PNG_LIMIT and 0 <= kind <= 8):
                            raise ValueError('Browser request exceeds fixture limits')
                        if kind == 2:
                            result = browser.crash()
                            browser = None
                            print(f'BROWSER_CRASH generation={tab} resize={resize} exit={result}', flush=True)
                            continue
                        if browser is None:
                            replacement = profile / 'replacement'
                            replacement.mkdir()
                            browser = Chromium(engine, replacement)
                            restored = browser.command('Browser.getVersion', browser=True)['product']
                            if restored != version:
                                raise RuntimeError('Replacement engine version differs')
                            generation = 0
                            print(f'BROWSER_RESTART {restored}', flush=True)
                        if tab != generation:
                            browser.open_target()
                            generation = tab
                        if kind >= 5:
                            prefix,(focused,preedit,committed,expected) = browser.edit(kind,raw,begin,end)
                            png,_ = browser.screenshot(width,height,2)
                            peer.sendall(prefix+png)
                            print(f'BROWSER_EDIT kind={kind} focus={focused} preedit={preedit} '
                                  f'committed={committed} expected={expected}', flush=True)
                            continue
                        png, (css_w, css_h, dpr) = browser.capture(width, height, kind)
                        peer.sendall(png)
                        print(f'BROWSER_FRAME generation={tab} resize={resize} kind={kind} '
                              f'css={css_w}x{css_h} dpr={dpr} pixels={width}x{height}', flush=True)
        except KeyboardInterrupt:
            pass
        except Exception:
            current = profile / ('replacement/chromium.stderr' if (profile / 'replacement').exists() else 'chromium.stderr')
            with current.open('rb') as log:
                size = log.seek(0, os.SEEK_END)
                log.seek(max(0, size - 6000))
                print(log.read().decode(errors='replace'), file=sys.stderr)
            raise
        finally:
            signal.signal(signal.SIGTERM, signal.SIG_IGN)
            if browser:
                result = browser.close()
                print(f'BROWSER_EXIT result={result}', flush=True)
                if result:
                    raise RuntimeError('Browser failed to close cleanly')


if __name__ == '__main__':
    if sys.argv[1] == '--launch':
        fd = int(sys.argv[2])
        os.dup2(fd, 3, inheritable=True)
        os.dup2(fd, 4, inheritable=True)
        if fd not in (3, 4):
            os.close(fd)
        os.execv(sys.argv[3], sys.argv[3:])
    else:
        serve(sys.argv[1], Path(sys.argv[2]))
