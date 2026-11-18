"""One owner-scoped TCP service on the private QEMU network fixture."""

import socket
import threading


NETWORK_OFF, NETWORK_PENDING, NETWORK_ACTIVE, NETWORK_REVOKED, NETWORK_UNKNOWN = range(1,6)
NETWORK_ENDPOINT = '10.0.2.100:8080'
NETWORK_BACKEND = 'nexaui-net'
NETWORK_ADAPTER = 'nexaui-nic'
NETWORK_MAC = '52:54:00:4e:58:01'
ERRORS = (OSError, ValueError, RuntimeError, EOFError, KeyError)


class GuestNetwork:
    def __init__(self, root):
        self.address = root / 'net-probe.sock'
        self.state = NETWORK_OFF
        self.thread = None
        self.bound = False
        self.stopping = threading.Event()
        self.connected = threading.Event()

    def serve(self, listener):
        try:
            with listener:
                while not self.stopping.is_set():
                    try:
                        peer, _ = listener.accept()
                    except TimeoutError:
                        continue
                    break
                else:
                    return
                with peer:
                    peer.settimeout(.1)
                    self.connected.set()
                    pending = bytearray()
                    while not self.stopping.is_set():
                        try:
                            data = peer.recv(16-len(pending))
                        except TimeoutError:
                            continue
                        if not data:
                            return
                        pending.extend(data)
                        while not self.stopping.is_set() and b'\n' in pending:
                            request, _, remainder = pending.partition(b'\n')
                            pending = remainder
                            if request not in (b'first',b'second'):
                                return
                            peer.sendall(b'NEXAUI ' + bytes(request) + b'\n')
                            print(f'GUEST_NETWORK_SERVICE {request.decode()}',flush=True)
                        if len(pending)==16:
                            return
        except OSError:
            pass
        finally:
            self.connected.clear()

    def grant(self, command):
        if self.state!=NETWORK_OFF:
            return
        self.state = NETWORK_UNKNOWN
        listener = socket.socket(socket.AF_UNIX,socket.SOCK_STREAM)
        try:
            listener.bind(str(self.address))
            self.bound = True
            listener.listen(1)
            listener.settimeout(.1)
            service = threading.Thread(target=self.serve,args=(listener,),daemon=True)
            service.start()
            self.thread = service
            command('netdev_add',type='user',id=NETWORK_BACKEND,restrict=True,ipv6=False,
                    guestfwd=[{'str':f'tcp:{NETWORK_ENDPOINT}-unix:{self.address}'}])
            command('device_add',driver='virtio-net-pci',id=NETWORK_ADAPTER,bus='pci.0',
                    netdev=NETWORK_BACKEND,mac=NETWORK_MAC)
            self.state = NETWORK_PENDING
            print('GUEST_NETWORK_GRANT_REQUEST',flush=True)
        except ERRORS:
            if not self.thread:
                listener.close()
            self.close()

    def observe(self, command):
        if self.state not in (NETWORK_PENDING,NETWORK_ACTIVE):
            return
        adapter = f'/machine/peripheral/{NETWORK_ADAPTER}'
        try:
            if not self.thread.is_alive():
                raise RuntimeError('Private network service exited')
            if not self.connected.is_set():
                return
            if (not command('qom-get',path=adapter,property='realized') or
                    command('qom-get',path=adapter,property='netdev')!=NETWORK_BACKEND or
                    command('qom-get',path=adapter,property='mac')!=NETWORK_MAC or
                    not self.connected.is_set()):
                raise ValueError('Private adapter scope changed')
            if self.state==NETWORK_PENDING:
                print('GUEST_NETWORK_ACTIVE',flush=True)
            self.state = NETWORK_ACTIVE
        except ERRORS:
            self.state = NETWORK_UNKNOWN
            self.close()

    def revoke(self, command):
        if self.state not in (NETWORK_PENDING,NETWORK_ACTIVE):
            return
        if not self.close():
            self.state = NETWORK_UNKNOWN
            return
        self.state = NETWORK_REVOKED
        print('GUEST_NETWORK_REVOKED',flush=True)
        try:
            command('set_link',name=NETWORK_ADAPTER,up=False)
            print('GUEST_NETWORK_LINK_DOWN_REQUESTED',flush=True)
        except ERRORS:
            pass

    def close(self):
        self.stopping.set()
        if self.thread:
            self.thread.join(timeout=1)
            if self.thread.is_alive():
                return False
        if self.bound:
            self.address.unlink(missing_ok=True)
            self.bound = False
        return True
