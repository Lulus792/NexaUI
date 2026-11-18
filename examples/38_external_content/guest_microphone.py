"""A private synthetic PCM input for the disposable QEMU desktop guest."""

import fcntl
import os
import struct
import threading


MIC_OFF, MIC_ACTIVE, MIC_REVOKED, MIC_UNKNOWN = range(1, 5)
PIPE_BYTES = 4096
TONE = struct.pack('<192h', *([12000, 12000] * 48 + [-12000, -12000] * 48)) * 5
SILENCE = bytes(len(TONE))


class GuestMicrophone:
    def __init__(self, root):
        self.lock = threading.Lock()
        self.stop = threading.Event()
        self.state = MIC_OFF
        self.config = root / 'microphone.conf'
        self.read_fd, self.write_fd = os.pipe2(os.O_NONBLOCK | os.O_CLOEXEC)
        try:
            self.pipe_bytes = fcntl.fcntl(self.write_fd, fcntl.F_SETPIPE_SZ, PIPE_BYTES)
            self.config.write_text(
                'pcm.null { type null }\n'
                'pcm.nexa_mic { type file slave.pcm "null" file "/dev/null" '
                f'infile {self.read_fd} format "raw" }}\n', encoding='ascii')
            self.feeder = threading.Thread(target=self.feed, name='guest-microphone')
            self.feeder.start()
        except BaseException:
            os.close(self.write_fd)
            os.close(self.read_fd)
            raise

    def feed(self):
        while not self.stop.is_set():
            with self.lock:
                try:
                    os.write(self.write_fd, TONE if self.state == MIC_ACTIVE else SILENCE)
                except BlockingIOError:
                    pass  # The bounded pipe already holds the next samples.
                except OSError:
                    self.state = MIC_UNKNOWN
                    return
            self.stop.wait(.004)

    def snapshot(self):
        with self.lock:
            return self.state

    def grant(self):
        with self.lock:
            if self.state == MIC_OFF:
                self.state = MIC_ACTIVE
                print('GUEST_MICROPHONE_ACTIVE source=synthetic', flush=True)

    def revoke(self):
        with self.lock:
            if self.state != MIC_ACTIVE:
                return
            # Stop producing the tone and discard any samples still in the
            # pipe. Samples already read by QEMU may still be in flight.
            self.state = MIC_REVOKED
            try:
                os.read(self.read_fd, self.pipe_bytes)
            except BlockingIOError:
                pass
            except OSError:
                self.state = MIC_UNKNOWN
                return
            print('GUEST_MICROPHONE_REVOKED', flush=True)

    def close(self):
        self.stop.set()
        self.feeder.join()
        os.close(self.write_fd)
        os.close(self.read_fd)
