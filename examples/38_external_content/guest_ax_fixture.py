"""A fullscreen GTK test window that exports native guest semantics."""

import gi

gi.require_version('Gtk', '3.0')
gi.require_version('Gdk', '3.0')
from gi.repository import Gdk, Gtk


window = Gtk.Window(title='Guest accessibility check', decorated=False)
box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=20)
box.set_margin_top(60)
box.set_margin_bottom(60)
box.set_margin_start(80)
box.set_margin_end(80)
status = Gtk.Label(label='Gaststatus – bereit')
button = Gtk.Button(label='Inspect guest job')
disabled = Gtk.Button(label='Disabled guest command')
disabled.set_sensitive(False)
entry = Gtk.Entry()
entry.set_visibility(False)
entry.set_text('private-example-entry')
for widget in (status, button, disabled, entry):
    box.pack_start(widget, True, True, 0)


def press(_window, event):
    if event.keyval == Gdk.KEY_F8:
        status.set_text('Gaststatus – fertig')
        print('NEXAUI_GUEST_AX_UPDATED', flush=True)
        return True
    if event.keyval == Gdk.KEY_F9:
        window.hide()
        Gtk.main_quit()
        return True
    return False


window.add(box)
window.connect('key-press-event', press)
window.fullscreen()
window.show_all()
with open('/run/nexaui-guest-ax-shown', 'xb'):
    pass
print('NEXAUI_GUEST_AX_APP_STARTED', flush=True)
Gtk.main()
print('NEXAUI_GUEST_AX_APP_EXITED', flush=True)
