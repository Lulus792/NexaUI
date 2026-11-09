"""Scope the private transfer adapter to independent foreground wl-copy children."""
import hashlib,os,re,select,socket,struct,subprocess
from pathlib import Path

def task_type(base):
    class ForeignStreamTask(base):
        def __init__(self,*args,gate):
            super().__init__(*args);self.gate=gate.resolve(strict=True);self.controls={};self.states={};self.pipes={}
            self.result['gate_library']=dict(path=str(self.gate),sha256=hashlib.sha256(self.gate.read_bytes()).hexdigest())
            self.result['stream_events']=[]

        def offer(self,payload):
            assert len(payload)>3 and b'\0' not in payload
            left,right=socket.socketpair(socket.AF_UNIX,socket.SOCK_SEQPACKET)
            stream=(self.root/('clipboard-source-start-'+str(len(self.sources))+'.log')).open('xb');self.streams.append(stream)
            env=dict(self.env,LD_PRELOAD=str(self.gate),NEXAUI_FOREIGN_GATE_FD=str(right.fileno()),
                NEXAUI_FOREIGN_GATE_PREFIX='3',NEXAUI_FOREIGN_GATE_PAYLOAD=payload.decode('utf-8'))
            try:
                child=subprocess.Popen([*self.command('wl-copy'),'--foreground','--type','text/plain;charset=utf-8','--',payload.decode('utf-8')],
                    env=env,pass_fds=(right.fileno(),),stdin=subprocess.DEVNULL,stdout=stream,stderr=subprocess.STDOUT)
            except BaseException:left.close();raise
            finally:right.close()
            self.sources.append(child);self.controls[child.pid]=left;self.states[child.pid]=dict(total=len(payload))
            Path(stream.name).rename(self.root/('clipboard-source-'+str(child.pid)+'.log'))
            def published():
                assert child.poll() is None,(child.returncode,self.source_log(child))
                return '.set_selection(' in self.source_log(child) and re.search(r'->\s*wl_surface#\d+\.destroy\(',self.source_log(child)) is not None
            self.wait(published,'gated foreign selection published and popup retired')
            assert child.poll() is None and str(self.gate) in Path(f'/proc/{child.pid}/maps').read_text()
            # The first ordinary transfer verifies the unchanged foreign bytes
            # before the parent explicitly arms one later request.
            self.read(payload);return child

        def arm(self,child):
            assert child.poll() is None;assert self.controls[child.pid].send(b'A')==1

        def message(self,child,stage):
            channel=self.controls[child.pid];assert select.select([channel],[],[],3)[0],'foreign stream gate response timed out'
            packet=channel.recv(41);assert len(packet)==40
            magic,actual,total,written,error,fd,queued=struct.unpack('=IIQQiiQ',packet)
            assert magic==0x4e584753 and actual==stage and total==self.states[child.pid]['total'] and fd>2
            row=dict(pid=child.pid,stage=stage,total=total,written=written,error=error,pipe_fd=fd,queued=queued)
            path=Path(f'/proc/{child.pid}/fd/{fd}')
            if stage in (1,2,5):
                assert not error and child.poll() is None;pipe=os.readlink(path);assert pipe.startswith('pipe:[')
                if child.pid in self.pipes:assert pipe==self.pipes[child.pid]
                else:self.pipes[child.pid]=pipe
                row['writer_pipe']=pipe
            elif stage==3:
                try:current=os.readlink(path)
                except FileNotFoundError:current=None
                assert current!=self.pipes[child.pid],'original writer pipe must actually be closed'
                row['writer_pipe_closed']=True
            self.states[child.pid]=row;self.result['stream_events'].append(row);return row

        def send(self,child,command):
            assert command in (b'N',b'E',b'P') and self.controls[child.pid].send(command)==1

        def drained(self,child):
            self.send(child,b'P');return self.message(child,5)['queued']==0

        def held(self,child):
            row=self.states[child.pid];assert child.poll() is None
            assert os.readlink(f'/proc/{child.pid}/fd/{row["pipe_fd"]}')==self.pipes[child.pid]
            return dict(pid=child.pid,writer_pipe=self.pipes[child.pid],written=row['written'],stage=row['stage'])

        def close(self):
            super().close()
            for channel in self.controls.values():channel.close()
            self.controls.clear()
    return ForeignStreamTask
