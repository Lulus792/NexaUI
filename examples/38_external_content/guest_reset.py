"""Disk identity and committed reset receipts for the private QEMU fixture."""

import hashlib
import os
import stat
import struct
import tempfile

WORK_DISK_BYTES = 16 * 1024 * 1024
RESET_APPLIED, RESET_REJECTED, RESET_UNKNOWN, RESET_UNREQUESTED = range(1, 5)
RESET_SCOPE_BYTES = 48
RECEIPT_NAME = 'reset.receipt'
RECEIPT = struct.Struct(f'>4s{RESET_SCOPE_BYTES}s6Q32s')


def image_identity(info):
    if not stat.S_ISREG(info.st_mode) or info.st_size != WORK_DISK_BYTES:
        raise ValueError('Unexpected work image')
    return info.st_dev, info.st_ino, info.st_size


def image_snapshot(path):
    fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    with os.fdopen(fd, 'rb') as image:
        identity = image_identity(os.fstat(image.fileno()))
        digest = hashlib.sha256()
        remaining = WORK_DISK_BYTES
        while remaining:
            data = image.read(min(remaining, 256 * 1024))
            if not data:
                raise ValueError('Work image became shorter')
            digest.update(data)
            remaining -= len(data)
        if image.read(1):
            raise ValueError('Work image grew')
        return identity, digest.digest()


def image_digest(path):
    return image_snapshot(path)[1]


def sync_directory(path):
    fd = os.open(path, os.O_RDONLY | os.O_DIRECTORY)
    try:
        os.fsync(fd)
    finally:
        os.close(fd)


def record_reset(directory, scope, replacement, baseline, previous, previous_hash):
    if len(scope)!=RESET_SCOPE_BYTES:
        raise ValueError('Invalid reset scope length')
    data = RECEIPT.pack(b'NXC1', scope, *replacement[:2], *baseline[:2], *previous[:2], previous_hash)
    receipt = directory / RECEIPT_NAME
    with tempfile.NamedTemporaryFile(dir=directory, prefix='receipt-') as output:
        output.write(data)
        output.flush()
        os.fsync(output.fileno())
        os.link(output.name, receipt, follow_symlinks=False)
    try:
        sync_directory(directory)
    except OSError:
        receipt.unlink(missing_ok=True)
        raise


def observe_reset(directory, scope):
    try:
        fd = os.open(directory / RECEIPT_NAME, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
        with os.fdopen(fd, 'rb') as receipt:
            info = os.fstat(receipt.fileno())
            if not stat.S_ISREG(info.st_mode) or info.st_size != RECEIPT.size:
                return RESET_UNKNOWN
            magic, recorded_scope, work_dev, work_ino, base_dev, base_ino, old_dev, old_ino, previous_hash = (
                RECEIPT.unpack(receipt.read(RECEIPT.size + 1)))
        if magic != b'NXC1':
            return RESET_UNKNOWN
        if scope != recorded_scope:
            return RESET_REJECTED
        checkpoint = recorded_scope[16:]
        expected = (
            ('work.img', (work_dev, work_ino, WORK_DISK_BYTES), checkpoint),
            ('baseline.img', (base_dev, base_ino, WORK_DISK_BYTES), checkpoint),
            ('work-before-reset.img', (old_dev, old_ino, WORK_DISK_BYTES), previous_hash),
        )
        if any(image_snapshot(directory / name) != (identity, digest) for name, identity, digest in expected):
            return RESET_UNKNOWN
    except (OSError, ValueError, struct.error):
        return RESET_UNKNOWN
    return RESET_APPLIED
