"""A fullscreen GTK test window that exports native guest semantics."""

import sys

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


def show_pointer_view():
    overlay = Gtk.Overlay()
    viewport = Gtk.ScrolledWindow()
    viewport.set_kinetic_scrolling(False)
    grid = Gtk.Grid()
    for index, color in enumerate(('#324968', '#5a375e', '#2d6055', '#674e30')):
        tile = Gtk.EventBox()
        tile.set_size_request(800, 600)
        style = Gtk.CssProvider()
        style.load_from_data(f'* {{ background-color: {color}; }}'.encode())
        tile.get_style_context().add_provider(style, Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION)
        grid.attach(tile, index % 2, index // 2, 1, 1)
    viewport.add(grid)
    overlay.add(viewport)

    indicator = Gtk.EventBox()
    indicator.set_size_request(96, 96)
    indicator.set_halign(Gtk.Align.START)
    indicator.set_valign(Gtk.Align.START)
    indicator.set_margin_top(16)
    indicator.set_margin_start(16)
    css = Gtk.CssProvider()
    indicator.get_style_context().add_provider(css, Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION)
    overlay.add_overlay(indicator)
    overlay.set_overlay_pass_through(indicator, True)

    def show_state(name, color):
        css.load_from_data(f'* {{ background-color: {color}; }}'.encode())
        print(f'NEXAUI_GUEST_VIEW_{name}', flush=True)

    def watch_axis(adjustment, direction, forward, backward):
        previous = adjustment.get_value()

        def changed(axis):
            nonlocal previous
            current = axis.get_value()
            if current != previous:
                show_state(f'{direction}_{"INCREASE" if current > previous else "DECREASE"}',
                           forward if current > previous else backward)
                previous = current

        adjustment.connect('value-changed', changed)

    held = set()

    def pointer_button(_widget, event):
        button = event.button
        if button not in (8, 9):
            return False
        if event.type == Gdk.EventType.BUTTON_PRESS and button not in held:
            held.add(button)
            show_state(f'BUTTON_{button}_DOWN', '#963182' if button == 8 else '#9c4228')
        elif event.type == Gdk.EventType.BUTTON_RELEASE and button in held:
            held.remove(button)
            show_state(f'BUTTON_{button}_UP', '#2e7091' if button == 8 else '#286940')
        return True

    window.connect('button-press-event', pointer_button)
    window.connect('button-release-event', pointer_button)
    window.remove(box)
    window.add(overlay)
    overlay.show_all()
    watch_axis(viewport.get_hadjustment(), 'X', '#235e9b', '#146d74')
    watch_axis(viewport.get_vadjustment(), 'Y', '#a14929', '#676023')
    show_state('READY', '#292b40')


def press(_window, event):
    if event.keyval == Gdk.KEY_F7 and window.get_child() == box:
        show_pointer_view()
        return True
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
window.add_events(Gdk.EventMask.BUTTON_PRESS_MASK | Gdk.EventMask.BUTTON_RELEASE_MASK)
window.connect('key-press-event', press)
with open('/run/nexaui-guest-ax-prepared', 'xb'):
    pass
if sys.stdin.buffer.read(1) != b'1':
    raise SystemExit(1)
window.fullscreen()
window.show_all()
with open('/run/nexaui-guest-ax-shown', 'xb'):
    pass
print('NEXAUI_GUEST_AX_APP_STARTED', flush=True)
Gtk.main()
print('NEXAUI_GUEST_AX_APP_EXITED', flush=True)
