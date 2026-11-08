"""Independent wl-clipboard task for the private native Files runner only."""
import hashlib
import os
from pathlib import Path
import re
import signal
import struct
import subprocess


class ClipboardTask:
    def __init__(self,root,tools,env,key,text,snapshot,state,native_case,wait,capture,result,app):
        self.root=root;self.tools=tools.resolve(strict=True);self.env=env
        self.key=key;self.text=text;self.snapshot=snapshot;self.state=state
        self.native_case=native_case;self.wait=wait;self.capture=capture;self.result=result;self.app=app
        self.sources=[];self.streams=[]
        expected={'lib/ld-musl-x86_64.so.1':'38d022ce7425ff105ccfb53598f606e6e5f5f0a34bfbc793d65e6f34c9d72806',
                  'bin/wl-copy':'75f5428d5a36c0c9d06611d4020f580a4622400e2f64579ecb45644dc888a921',
                  'bin/wl-paste':'7221ecacd258bf00989f30bff9ea02bc3cac17a588150f4bf81a8b0253ef9356'}
        inputs={}
        for p in sorted(self.tools.rglob('*')):
            if p.is_file() and not p.is_symlink():inputs[p.relative_to(self.tools).as_posix()]=hashlib.sha256(p.read_bytes()).hexdigest()
        assert all(inputs.get(p)==digest for p,digest in expected.items()),'Changed independent clipboard tools'
        result['clipboard_tool_inputs']=inputs;result['clipboard_checks']=[]

    def command(self,name):
        return [str(self.tools/'lib/ld-musl-x86_64.so.1'),'--library-path',str(self.tools/'lib'),str(self.tools/'bin'/name)]

    def source_log(self,child):
        return (self.root/('clipboard-source-'+str(child.pid)+'.log')).read_text(errors='replace')

    def read(self,expected):
        done=subprocess.run([*self.command('wl-paste'),'--no-newline'],env=self.env,capture_output=True,timeout=3)
        self.result['clipboard_checks'].append(dict(foreign_read_exit=done.returncode,length=len(done.stdout),sha256=hashlib.sha256(done.stdout).hexdigest()))
        assert done.returncode==0 and done.stdout==expected,(done.returncode,done.stderr)

    def offer(self,payload):
        # Synthetic public fixture text only. Argument input avoids stdin spools.
        stream=(self.root/('clipboard-source-start-'+str(len(self.sources))+'.log')).open('xb');self.streams.append(stream)
        child=subprocess.Popen([*self.command('wl-copy'),'--foreground','--type','text/plain;charset=utf-8','--',payload.decode('utf-8')],env=self.env,stdin=subprocess.DEVNULL,stdout=stream,stderr=subprocess.STDOUT)
        self.sources.append(child);old=Path(stream.name);new=self.root/('clipboard-source-'+str(child.pid)+'.log');old.rename(new)
        self.wait(lambda:'.set_selection(' in self.source_log(child) and re.search(r'->\s*wl_surface#\d+\.destroy\(',self.source_log(child)) is not None,'foreign selection published and popup retired')
        assert child.poll() is None;self.read(payload);return child

    def expect(self,value,cursor,selection=None):
        self.text('Name',value,cursor,selection);self.snapshot(value,cursor,selection)

    def clear(self):
        self.key(22,True);self.expect('',0)

    def seed(self):
        self.clear();self.key(31);self.expect('s',1)

    def send_returned(self,child,before):
        sends=len(re.findall(r'wl_data_source#\d+\.send\(',self.source_log(child)))
        if sends<=before:return False
        try:
            raw=Path(f'/proc/{child.pid}/syscall').read_text().split();fds=[]
            if raw[0] in ('0','47'):fds=[int(raw[1],0)]
            elif raw[0] in ('7','271'):
                address,count=int(raw[1],0),int(raw[2],0)
                if not 1<=count<=16:return False
                with Path(f'/proc/{child.pid}/mem').open('rb',buffering=0) as memory:data=os.pread(memory.fileno(),count*8,address)
                if len(data)!=count*8:return False
                fds=[fd for fd,events,_ in struct.iter_unpack('<ihh',data) if events&1]
            else:return False
            sockets={fd:os.readlink(f'/proc/{child.pid}/fd/{fd}') for fd in fds}
            if not sockets or not all(p.startswith('socket:[') for p in sockets.values()):return False
            self.result['clipboard_checks'].append(dict(send_return_witness=raw,sockets=sockets,sends=sends));return True
        except (FileNotFoundError,ProcessLookupError,IndexError):return False

    def pending(self,payload):
        child=self.offer(payload);child.send_signal(signal.SIGSTOP)
        self.wait(lambda:re.search(r'^State:\s+T',Path(f'/proc/{child.pid}/status').read_text(),re.M) is not None,'foreign clipboard source actually stopped')
        before=len(re.findall(r'wl_data_source#\d+\.send\(',self.source_log(child)))
        log=self.root/'app.log';cut=len(log.read_bytes());self.key(47,True)
        self.wait(lambda:'.receive(' in log.read_text(errors='replace')[cut:],'native clipboard receive requested while producer stopped')
        return child,before

    def run(self,budget):
        sample='Grüße 🌍 é';raw=sample.encode();self.clear();owner=self.offer(raw)
        self.key(47,True);self.expect(sample,len(sample));self.capture('foreign Unicode clipboard pasted into original filename')
        self.key(30,True);log=self.root/'app.log';cut=len(log.read_bytes());self.key(46,True)
        self.wait(lambda:'.set_selection(' in log.read_text(errors='replace')[cut:] and owner.poll()==0,'Files copy replaced foreign clipboard ownership')
        self.clear();self.read(raw);self.expect('',0)
        self.key(47,True);self.expect(sample,len(sample));self.key(30,True);self.key(45,True);self.expect('',0);self.read(raw)
        self.key(44,True);self.expect(sample,len(sample),(0,len(sample)));self.key(21,True);self.expect('',0)
        self.offer(b'Assist');self.key(47,True);self.expect('Assist',6)
        self.native_case('clipboard-copy');self.clear();self.read(b'Assist')
        self.native_case('clipboard-paste');self.expect('Assist',6)
        # The preceding foreign receiver used a temporary focus popup. A fresh
        # native key supplies this window's selection serial before native Cut.
        self.key(42);self.native_case('clipboard-cut');self.expect('',0);self.read(b'Assist')
        self.key(44,True);self.expect('Assist',6);self.key(21,True);self.expect('',0)
        self.capture('independent AT-SPI clipboard copy cut and paste')
        # Insertion in the middle must retain the actual selected range on undo.
        self.offer('aébc'.encode());self.key(47,True);self.expect('aébc',4)
        self.key(102);self.key(106);self.key(106,shift=True);self.expect('aébc',2,(1,2))
        self.offer(b'Z');self.key(47,True);self.expect('aZbc',2)
        self.key(44,True);self.expect('aébc',2,(1,2));self.key(21,True);self.expect('aZbc',2)
        self.capture('foreign replacement and one undo preserve selection')
        for payload in (b'bad/name',b'x'*256,*([b'x'*65537] if budget else [])):
            self.seed();self.offer(payload);self.key(47,True)
            self.wait(lambda:self.state(name_bad=1),'whole foreign clipboard proposal rejected '+str(len(payload)))
            self.text('Name','s',1);self.native_case('invalid');self.key(28)
            assert self.state(dialog=1,name_bad=1)
        self.capture('oversized or invalid foreign paste retains prefix and blocks confirmation')
        self.seed();self.offer(b'name\nline');self.key(30,True);self.key(47,True);self.expect('name line',9)
        self.result['clipboard_checks'].append('Single-line newline-to-space conversion follows the existing SDK clipboard contract')
        self.seed();child,before=self.pending(b'late');self.text('Name','s',1);self.key(15);child.send_signal(signal.SIGCONT)
        self.wait(lambda:self.send_returned(child,before),'late foreign send returned before checking retired focus')
        self.key(42);self.text('Name','s',1);self.key(15,shift=True);self.expect('s',1)
        self.result['clipboard_checks'].append('Late completed paste after Tab did not change the original draft')
        self.clear()

    def pending_close(self):
        # The preceding IME task intentionally leaves active preedit. Retire it
        # through the real native disable/reactivate exchange before Ctrl+N.
        server=self.root/'server.log';cut=len(server.read_bytes());self.key(1)
        self.wait(lambda:'IME commit enabled=0 ' in server.read_text()[cut:] and
                  server.read_text().rfind('IME commit enabled=1 ')>server.read_text().rfind('IME commit enabled=0 '),
                  'preedit actually cancelled before pending clipboard close')
        self.key(49,True);self.wait(lambda:self.state(dialog=1),'fresh naming scope before pending clipboard close')
        self.text('Name','',0);self.snapshot('',0);self.close_source=self.pending(b'close pending')[0]
        self.result['clipboard_checks'].append('App close requested while independent clipboard producer remains stopped')

    def closed(self):
        assert self.app().returncode==0 and self.close_source.poll() is None
        assert re.search(r'^State:\s+T',Path(f'/proc/{self.close_source.pid}/status').read_text(),re.M)
        self.result['clipboard_checks'].append('App exited 0 before the stopped foreign producer was resumed')

    def close(self):
        for child in reversed(self.sources):
            if child.poll() is None:
                child.send_signal(signal.SIGCONT);child.terminate()
                try:child.wait(timeout=5)
                except subprocess.TimeoutExpired:child.kill();child.wait(timeout=5)
        for stream in self.streams:stream.close()
