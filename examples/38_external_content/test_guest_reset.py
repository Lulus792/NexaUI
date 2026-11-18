"""Regression checks for the observation-only reset owner."""

import os
from pathlib import Path
import shutil
import socket
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

from guest_reset import (RECEIPT_NAME, RESET_APPLIED, RESET_REJECTED, RESET_UNKNOWN, WORK_DISK_BYTES,
                         image_snapshot, observe_reset, record_reset)


class ResetRecoveryTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='nexaui-guest-')
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.work = self.root / 'work.img'
        self.baseline = self.root / 'baseline.img'
        self.previous = self.root / 'work-before-reset.img'
        self.receipt = self.root / RECEIPT_NAME
        for path in (self.work,self.baseline,self.previous):
            with path.open('xb') as image:
                image.truncate(WORK_DISK_BYTES)
        with self.previous.open('r+b') as image:
            image.write(b'Previously saved guest data\n')
        self.replacement_id, checkpoint = image_snapshot(self.work)
        self.baseline_id, _ = image_snapshot(self.baseline)
        self.previous_id, self.previous_hash = image_snapshot(self.previous)
        self.scope = bytes(range(16)) + checkpoint

    def commit(self):
        record_reset(self.root,self.scope,self.replacement_id,self.baseline_id,self.previous_id,self.previous_hash)

    def test_missing_or_invalid_receipt(self):
        self.assertEqual(observe_reset(self.root,self.scope), RESET_UNKNOWN)
        self.commit()
        valid = self.receipt.read_bytes()
        for malformed in (valid[:-1], valid+b'\0', b'NXC0'+valid[4:]):
            with self.subTest(length=len(malformed), magic=malformed[:4]):
                self.receipt.write_bytes(malformed)
                self.assertEqual(observe_reset(self.root,self.scope), RESET_UNKNOWN)
        self.receipt.rename(self.root / 'moved.receipt')
        self.receipt.symlink_to(self.root / 'moved.receipt')
        self.assertEqual(observe_reset(self.root,self.scope), RESET_UNKNOWN)

    def test_existing_receipt_is_preserved(self):
        self.commit()
        receipt = self.receipt.read_bytes()
        with self.assertRaises(FileExistsError):
            self.commit()
        self.assertEqual(self.receipt.read_bytes(), receipt)
        self.assertEqual(observe_reset(self.root,self.scope), RESET_APPLIED)

    def test_failed_receipt_sync_is_unknown(self):
        with patch('guest_reset.sync_directory',side_effect=OSError('injected receipt sync failure')):
            with self.assertRaises(OSError):
                self.commit()
        self.assertFalse(self.receipt.exists())
        self.assertEqual(observe_reset(self.root,self.scope), RESET_UNKNOWN)

    def test_each_observed_image_requires_its_identity(self):
        self.commit()
        for path in (self.work,self.baseline,self.previous):
            with self.subTest(image=path.name):
                original = self.root / 'original.img'
                path.rename(original)
                shutil.copyfile(original,path)
                self.assertEqual(observe_reset(self.root,self.scope), RESET_UNKNOWN)
                os.replace(original,path)
                self.assertEqual(observe_reset(self.root,self.scope), RESET_APPLIED)

    def test_each_observed_image_requires_its_bytes(self):
        self.commit()
        for path in (self.work,self.baseline,self.previous):
            with self.subTest(image=path.name):
                with path.open('r+b') as image:
                    old = image.read(1)
                    image.seek(0)
                    image.write(bytes((old[0]^255,)))
                self.assertEqual(observe_reset(self.root,self.scope), RESET_UNKNOWN)
                with path.open('r+b') as image:
                    image.write(old)
                self.assertEqual(observe_reset(self.root,self.scope), RESET_APPLIED)

    def test_restarted_process_reads_and_refuses_apply(self):
        self.commit()
        address = self.root / 'guest.sock'
        with socket.socket(socket.AF_UNIX,socket.SOCK_STREAM) as listener:
            listener.bind(str(address))
            listener.listen(1)
            fd = listener.fileno()
            service = subprocess.Popen([sys.executable,str(Path(__file__).with_name('guest_fixture.py')),
                                        '--recover-reset',str(address),str(fd)],pass_fds=(fd,),stdout=subprocess.PIPE)
            try:
                def request(kind, scope):
                    with socket.socket(socket.AF_UNIX,socket.SOCK_STREAM) as peer:
                        peer.settimeout(5)
                        peer.connect(str(address))
                        peer.sendall(f'9 3 88 66 {kind} {len(scope)}\n'.encode()+scope)
                        peer.shutdown(socket.SHUT_WR)
                        with peer.makefile('rb') as reply:
                            return reply.read()

                self.assertEqual(request(10,self.scope), b'NXA1'+bytes((RESET_APPLIED,)))
                self.assertEqual(request(10,self.scope[:-1]), b'')
                self.assertEqual(request(10,b'X'+self.scope[1:]), b'NXA1'+bytes((RESET_REJECTED,)))
                self.assertEqual(request(9,self.scope), b'NXA1'+bytes((RESET_REJECTED,)))
                self.assertEqual(request(10,self.scope), b'NXA1'+bytes((RESET_APPLIED,)))
            finally:
                service.terminate()
                try:
                    output, _ = service.communicate(timeout=6)
                except subprocess.TimeoutExpired:
                    service.kill()
                    service.communicate()
                    raise
            self.assertEqual(service.returncode,0,output)
            self.assertIn(b'GUEST_RESET_RECOVERY kind=9 state=2',output)
        self.assertEqual(image_snapshot(self.work)[0],self.replacement_id)
        self.assertEqual(image_snapshot(self.previous),(self.previous_id,self.previous_hash))


if __name__ == '__main__':
    unittest.main()
