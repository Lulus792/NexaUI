"""Actual pipe bytes/EOF and child outcomes, independent of the UI implementation."""
import argparse,errno,os,select,socket,struct,subprocess

def main():
    p=argparse.ArgumentParser();p.add_argument('gate');p.add_argument('writer');p.add_argument('--preload-first');args=p.parse_args()
    payload='é🌍\r\nlate'.encode();checks=[]
    for case in ('ordinary','held','truncated','reader-closed'):
        left,right=socket.socketpair(socket.AF_UNIX,socket.SOCK_SEQPACKET);read_fd,write_fd=os.pipe()
        env=dict(os.environ,LD_PRELOAD=':'.join(x for x in (args.preload_first,args.gate) if x),NEXAUI_FOREIGN_GATE_FD=str(right.fileno()),NEXAUI_FOREIGN_GATE_PREFIX='3',NEXAUI_FOREIGN_GATE_PAYLOAD=payload.decode())
        if case!='ordinary':left.send(b'A')
        child=subprocess.Popen([args.writer,str(write_fd),'short' if case in ('truncated','reader-closed') else 'full'],env=env,pass_fds=(write_fd,right.fileno()),stdin=subprocess.DEVNULL,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        right.close();os.close(write_fd)
        def message(stage,written,error=0,queued=0):
            assert select.select([left],[],[],3)[0],(case,'missing gate message')
            data=left.recv(41);assert len(data)==40
            magic,actual,total,done,failure,fd,remaining=struct.unpack('=IIQQiiQ',data)
            assert (magic,actual,total,done,failure)==(0x4e584753,stage,len(payload),written,error),(case,data)
            assert fd>2
            if stage==5:assert remaining==queued
        def read(size):
            result=b''
            while len(result)<size:
                assert select.select([read_fd],[],[],3)[0];part=os.read(read_fd,size-len(result));assert part;result+=part
            return result
        try:
            if case=='ordinary':assert read(len(payload))==payload
            else:
                message(1,3);left.send(b'P');message(5,3,queued=3);assert read(3)==payload[:3]
                left.send(b'P');message(5,3)
                assert not select.select([read_fd],[],[],0)[0],'pipe must remain open with no additional bytes'
                if case=='reader-closed':
                    os.close(read_fd);read_fd=-1;left.send(b'N');message(4,3,errno.EPIPE);message(3,3,errno.EPIPE)
                elif case=='truncated':left.send(b'E');message(3,3)
                else:
                    left.send(b'N');message(2,len(payload));left.send(b'P');message(5,len(payload),queued=len(payload)-3)
                    assert read(len(payload)-3)==payload[3:];left.send(b'P');message(5,len(payload))
                    assert not select.select([read_fd],[],[],0)[0],'complete bytes alone must not produce EOF'
                    left.send(b'E');message(3,len(payload))
            out,err=child.communicate(timeout=3);assert child.returncode==0,(case,child.returncode,out,err)
            if read_fd>=0:assert os.read(read_fd,1)==b''
            assert not any(s in err for s in (b'AddressSanitizer',b'LeakSanitizer',b'runtime error:')),(case,err)
            checks.append(case)
        finally:
            if child.poll() is None:child.kill();child.wait(timeout=3)
            left.close()
            if read_fd>=0:os.close(read_fd)
    print('Foreign stream gate actual byte/EOF checks passed:',','.join(checks))

if __name__=='__main__':main()
