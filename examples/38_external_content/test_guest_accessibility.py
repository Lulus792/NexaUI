"""Socket and frame checks for the QEMU guest accessibility relay."""

from pathlib import Path
import select
import socket
import struct
import unittest
from unittest.mock import Mock, patch

from guest_accessibility import AX_LIMIT, REPLY, REQUEST, GuestAccessibility
from guest_fixture import Guest


def reply(ticket, payload):
    return REPLY.pack(ticket, len(payload)) + payload


class GuestAccessibilityTest(unittest.TestCase):
    def setUp(self):
        self.channel = GuestAccessibility(Path('.'))
        self.addCleanup(self.channel.close)
        self.channel.peer, self.guest = socket.socketpair()
        self.addCleanup(self.guest.close)
        self.guest.settimeout(.5)
        self.enterContext(patch('guest_accessibility.AX_REPLY_TIMEOUT', .04, create=True))

    def expect_ticket(self, ticket):
        self.assertEqual(self.guest.recv(REQUEST.size), REQUEST.pack(ticket))

    def expect_no_ticket(self):
        self.assertEqual(select.select([self.guest], [], [], 0)[0], [])

    def test_partial_header_is_drained_without_sending_another_request(self):
        late = reply(1, b'old tree')
        self.guest.sendall(late[:5])
        self.assertEqual(self.channel.read(), b'')
        self.expect_ticket(1)
        self.assertEqual(self.channel.read(), b'')
        self.expect_no_ticket()
        self.guest.sendall(late[5:] + reply(2, b'new tree'))
        self.assertEqual(self.channel.read(), b'new tree')
        self.expect_ticket(2)

    def test_video_only_until_fresh_snapshots_match_around_capture(self):
        owner = Guest.__new__(Guest)
        owner.accessibility = self.channel
        png = b'\x89PNG\r\n\x1a\nframe'
        owner.capture = Mock(return_value=png)
        late = reply(1, b'old tree')
        split = REPLY.size + 3
        self.guest.sendall(late[:split])
        self.assertEqual(owner.frame(), struct.pack('>I', 0) + png)
        self.expect_ticket(1)
        self.assertEqual(owner.frame(), struct.pack('>I', 0) + png)
        self.expect_no_ticket()

        new = b'new tree'
        self.guest.sendall(late[split:] + reply(2, new) + reply(3, new))
        self.assertEqual(owner.frame(), struct.pack('>I', len(new)) + new + png)
        self.expect_ticket(2)
        self.expect_ticket(3)
        self.guest.sendall(reply(4, new) + reply(5, b'replaced during capture'))
        self.assertEqual(owner.frame(), struct.pack('>I', 0) + png)
        self.expect_ticket(4)
        self.expect_ticket(5)

    def test_wrong_ticket_closes_channel(self):
        self.guest.sendall(reply(2, b'unmatched'))
        self.assertEqual(self.channel.read(), b'')
        self.expect_ticket(1)
        self.assertIsNone(self.channel.peer)

    def test_oversized_late_reply_closes_channel(self):
        self.assertEqual(self.channel.read(), b'')
        self.expect_ticket(1)
        self.assertIsNotNone(self.channel.peer)
        self.guest.sendall(REPLY.pack(1, AX_LIMIT + 1))
        self.assertEqual(self.channel.read(), b'')
        self.assertIsNone(self.channel.peer)

    def test_receive_eof_closes_channel(self):
        self.guest.shutdown(socket.SHUT_WR)
        self.assertEqual(self.channel.read(), b'')
        self.expect_ticket(1)
        self.assertIsNone(self.channel.peer)

    def test_ambiguous_partial_request_closes_channel(self):
        real_peer = self.channel.peer
        self.channel.peer = Mock(wraps=real_peer)

        def stall(data):
            real_peer.sendall(data[:4])
            raise TimeoutError('request write timed out')

        self.channel.peer.sendall.side_effect = stall
        self.assertEqual(self.channel.read(), b'')
        self.assertEqual(self.guest.recv(REQUEST.size), REQUEST.pack(1)[:4])
        self.assertIsNone(self.channel.peer)
        self.assertEqual(self.guest.recv(REQUEST.size), b'')


if __name__ == '__main__':
    unittest.main()
