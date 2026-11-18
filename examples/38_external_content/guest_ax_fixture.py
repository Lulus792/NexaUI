"""A fullscreen GTK test window for native guest semantics and input."""

import sys

import gi

gi.require_version('Gtk', '3.0')
gi.require_version('Gdk', '3.0')
from gi.repository import Gdk, Gtk


window = Gtk.Window(title='Guest accessibility check', decorated=False)
views = Gtk.Stack(homogeneous=False)
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


def create_text_view():
    view = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=16)
    view.set_margin_top(60)
    view.set_margin_start(80)
    view.set_margin_end(80)
    view.pack_start(Gtk.Label(label='Guest Unicode composition'), False, False, 0)
    field = Gtk.Entry()
    field.set_property('im-module', 'gtk-im-context-simple')
    view.pack_start(field, False, False, 0)

    changes = 0
    preedit = ''
    composed = '\u00e9'

    def report(stage):
        print(f'NEXAUI_GUEST_TEXT_{stage}', flush=True)

    def changed(widget):
        nonlocal changes
        changes += 1
        if changes == 1 and widget.get_text() == composed:
            report('COMMIT')

    def preedit_changed(widget, text):
        nonlocal preedit
        value = widget.get_text()
        if text == 'u00e9' and changes == 0 and value == '':
            report('PREEDIT')
        elif text == 'u00f1' and changes == 1 and value == composed:
            report('CANCEL_PREEDIT')
        elif preedit == 'u00f1' and text == '' and changes == 1 and value == composed:
            report('CANCELLED')
        preedit = text

    def leave(_window, event):
        if event.keyval != Gdk.KEY_F5 or views.get_visible_child() != view:
            return False
        if changes == 1 and preedit == '' and field.get_text() == composed:
            report('UNCHANGED')
        else:
            report('UNEXPECTED_EDIT')
        views.set_visible_child(box)
        button.grab_focus()
        return True

    def show():
        nonlocal changes, preedit
        field.set_text('')
        changes = 0
        preedit = ''
        views.set_visible_child(view)
        field.grab_focus()
        report('READY')

    field.connect('changed', changed)
    field.connect('preedit-changed', preedit_changed)
    window.connect('key-press-event', leave)
    views.add_named(view, 'composition')
    return show


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
    views.add_named(overlay, 'pointer')
    overlay.show_all()
    views.set_visible_child(overlay)
    watch_axis(viewport.get_hadjustment(), 'X', '#235e9b', '#146d74')
    watch_axis(viewport.get_vadjustment(), 'Y', '#a14929', '#676023')
    show_state('READY', '#292b40')


def press(_window, event):
    if event.keyval == Gdk.KEY_F6 and views.get_visible_child() == box:
        show_text_view()
        return True
    if event.keyval == Gdk.KEY_F7 and views.get_visible_child() == box:
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


views.add_named(box, 'accessibility')
show_text_view = create_text_view()
window.add(views)
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
