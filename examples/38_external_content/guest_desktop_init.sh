#!/bin/busybox sh
# Init for the optional, disposable Alpine/Weston console test guest.
set -eu
export PATH=/usr/sbin:/usr/bin:/sbin:/bin
/bin/busybox --install -s
mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t devtmpfs devtmpfs /dev
mount -t tmpfs tmpfs /run
mkdir -p /dev/pts /run/desktop
mount -t devpts devpts /dev/pts
chmod 700 /run/desktop

modprobe virtio_gpu
modprobe evdev
modprobe psmouse
udevd --daemon
udevadm trigger --action=add
udevadm settle --timeout=5
for event in /dev/input/event*; do
    if udevadm info -q property "$event" | grep -qx ID_INPUT_MOUSE=1; then
        evtest "$event" >/dev/ttyS0 2>&1 &
        break
    fi
done

export XDG_RUNTIME_DIR=/run/desktop WAYLAND_DISPLAY=wayland-guest
export LIBSEAT_BACKEND=seatd SEATD_VTBOUND=0
wait_socket() {
    tries=0
    until [ -S "$1" ]; do
        kill -0 "$2"
        tries=$((tries + 1))
        [ "$tries" -lt 100 ]
        sleep .1
    done
}
seatd &
wait_socket /run/seatd.sock "$!"
cat >/run/weston.ini <<'EOF'
[core]
idle-time=0
xwayland=false

[shell]
background-color=0xff193044
panel-color=0xff244762
locking=false

[output]
name=Virtual-1
mode=800x600

[launcher]
icon=/usr/share/weston/terminal.png
path=/usr/bin/weston-terminal
EOF

weston --backend=drm --renderer=pixman --socket="$WAYLAND_DISPLAY" --config=/run/weston.ini &
desktop=$!
wait_socket "$XDG_RUNTIME_DIR/$WAYLAND_DISPLAY" "$desktop"
export TERM=xterm SHELL=/bin/sh
cat >/run/console-probe <<'EOF'
#!/bin/sh
printf 'NexaUI console test\r\n'
printf '%s\n' NEXAUI_DESKTOP_READY >/dev/ttyS0
while IFS= read -r line; do
    if [ "$line" = nexa ]; then
        sleep .4
        printf '\033[42m\033[2J\033[H'
        for row in 1 2 3 4 5 6; do
            printf '%-60s\r\n' 'Guest keyboard input received'
        done
        printf '\033[0m'
        printf '%s\n' NEXAUI_DESKTOP_INPUT >/dev/ttyS0
    fi
done
EOF
chmod 755 /run/console-probe
weston-terminal --maximized --shell=/run/console-probe &
wait "$desktop"
