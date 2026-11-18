"""One read-only emulated USB disk owned by the disposable VM fixture."""

USB_OFF, USB_ATTACHING, USB_ACTIVE, USB_DETACHING, USB_REVOKED, USB_UNKNOWN = range(1, 7)
USB_CONTROLLER = 'nexaui-usb'
USB_DEVICE = 'nexaui-usb-test'
USB_NODE = 'nexaui-usb-disk'
USB_SERIAL = 'NexaUI-TestDisk'
USB_PATH = f'/machine/peripheral/{USB_DEVICE}'
USB_BUS_PATH = f'/machine/peripheral/{USB_CONTROLLER}/{USB_CONTROLLER}.0'
USB_SIZE = 1024 * 1024
ERRORS = (OSError, ValueError, RuntimeError, EOFError, KeyError)


class GuestUSB:
    def __init__(self, root):
        self.image = root / 'private-usb.raw'
        with self.image.open('xb') as disk:
            disk.write(b'NexaUI private USB test disk\n')
            disk.seek(512000)
            disk.write(b'NexaUI unseen sector')
            disk.truncate(USB_SIZE)
        self.image.chmod(0o400)
        self.state = USB_OFF
        self.removing = False
        self.deleted = False

    def event(self, reply):
        data = reply.get('data', {})
        if data.get('device') != USB_DEVICE or data.get('path') != USB_PATH:
            return
        if reply.get('event') == 'DEVICE_DELETED':
            self.deleted = True
        elif reply.get('event') == 'DEVICE_UNPLUG_GUEST_ERROR' and self.removing:
            self.state = USB_UNKNOWN
            self.removing = False

    def present(self, command):
        return any(entry['name'] == USB_DEVICE for entry in command('qom-list', path='/machine/peripheral'))

    def block(self, command):
        return [entry for entry in command('query-block')
                if entry.get('qdev', '').startswith(USB_PATH + '/') or
                entry.get('inserted', {}).get('node-name') == USB_NODE]

    def matches(self, command):
        for name, value in (('type', 'usb-storage'), ('realized', True), ('attached', True), ('port', '1'),
                            ('serial', USB_SERIAL), ('parent_bus', USB_BUS_PATH)):
            if command('qom-get', path=USB_PATH, property=name) != value:
                return False
        disk, = self.block(command)
        image = disk['inserted']
        return (disk.get('qdev', '').startswith(USB_PATH + '/') and image['node-name'] == USB_NODE and
                image['ro'] is True and image['file'] == str(self.image) and
                image['drv'] == 'raw' and image['image']['virtual-size'] == USB_SIZE)

    def grant(self, command):
        if self.state != USB_OFF:
            return
        self.state = USB_UNKNOWN
        try:
            if self.present(command):
                return
            command('blockdev-add', driver='raw', **{'node-name': USB_NODE, 'read-only': True},
                    file={'driver': 'file', 'filename': str(self.image), 'read-only': True})
            command('device_add', driver='usb-storage', id=USB_DEVICE, bus=f'{USB_CONTROLLER}.0',
                    port='1', drive=USB_NODE, serial=USB_SERIAL)
            self.state = USB_ATTACHING
            print('GUEST_USB_ATTACH_REQUEST', flush=True)
        except ERRORS:
            pass

    def revoke(self, command):
        if self.state != USB_ACTIVE:
            return
        self.state = USB_UNKNOWN
        try:
            if not self.present(command) or not self.matches(command):
                return
            self.removing = True
            self.deleted = False
            self.state = USB_DETACHING
            command('device_del', id=USB_DEVICE)
            print('GUEST_USB_DETACH_REQUEST', flush=True)
        except ERRORS:
            self.state = USB_UNKNOWN

    def observe(self, command):
        if self.state in (USB_OFF, USB_REVOKED):
            return
        try:
            present = self.present(command)
            if self.removing:
                if present or not self.deleted or self.block(command):
                    return
                nodes = command('query-named-block-nodes')
                if any(entry.get('node-name') == USB_NODE for entry in nodes):
                    command('blockdev-del', **{'node-name': USB_NODE})
                if any(entry.get('node-name') == USB_NODE for entry in command('query-named-block-nodes')):
                    raise ValueError('USB backing node still present')
                self.state = USB_REVOKED
                print('GUEST_USB_REVOKED backend=closed', flush=True)
            elif present and self.matches(command):
                if self.state != USB_ACTIVE:
                    print('GUEST_USB_ACTIVE access=read-only', flush=True)
                self.state = USB_ACTIVE
            elif self.state != USB_ATTACHING or present:
                self.state = USB_UNKNOWN
        except ERRORS:
            self.state = USB_UNKNOWN
