"""Private, revocable filesystem backend for the QEMU test guest."""

import errno
import os
from pathlib import Path
import stat
import subprocess
import time


def private_export_identity(path):
    info = path.stat(follow_symlinks=False)
    if not stat.S_ISDIR(info.st_mode) or stat.S_IMODE(info.st_mode)!=0o700 or info.st_uid!=os.geteuid():
        raise ValueError('File sharing requires the private export directory')
    return info.st_dev, info.st_ino


class GuestExport:
    def __init__(self, bindfs, source, mount):
        self.bindfs = bindfs
        self.source = source
        self.mount = mount
        self.process = None
        self.mount_id = None

    def mounted_id(self):
        with Path('/proc/self/mountinfo').open() as mounts:
            matches = []
            for row in mounts:
                fields, _, filesystem = row.partition(' - ')
                fields = fields.split()
                if len(fields)>4 and fields[4]==str(self.mount):
                    if filesystem.split()[:2]!=['fuse.bindfs',str(self.source)]:
                        raise RuntimeError('Unexpected filesystem at the private guest export')
                    matches.append(fields[0])
        if len(matches)>1:
            raise RuntimeError('Multiple mounts at the private guest export')
        return matches[0] if matches else None

    def __enter__(self):
        self.source_identity = private_export_identity(self.source)
        self.mount.mkdir(mode=0o700)
        try:
            self.process = subprocess.Popen(
                [self.bindfs,'-f','--no-allow-other','--direct-io','-o','attr_timeout=0,entry_timeout=0',
                 self.source,self.mount],stdin=subprocess.DEVNULL)
            deadline = time.monotonic()+4
            while self.process.poll() is None:
                self.mount_id = self.mounted_id()
                if self.mount_id:
                    return self
                if time.monotonic()>=deadline:
                    break
                time.sleep(.025)
            raise RuntimeError('Private guest file backend did not mount')
        except BaseException:
            self.close()
            raise

    def __exit__(self, *_):
        self.close()

    def status(self):
        if not self.mount_id or self.mounted_id()!=self.mount_id:
            return 'unknown'
        if self.process.poll() is None:
            return 'running'
        # Keep the disconnected mount in place until QEMU releases its handles.
        try:
            self.mount.stat()
        except OSError as error:
            if error.errno==errno.ENOTCONN:
                return 'cut off'
            raise
        return 'unknown'

    def cut_off(self):
        self.process.kill()

    def close(self):
        if self.process:
            self.process.kill()
            self.process.wait(timeout=4)
        mounted = self.mounted_id()
        if mounted:
            if self.mount_id and mounted!=self.mount_id:
                raise RuntimeError('Private guest file backend mount changed')
            subprocess.run(['fusermount3','-u',self.mount],check=True,timeout=4)
