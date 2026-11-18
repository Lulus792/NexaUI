#!/bin/busybox sh
# Init for the optional, disposable Alpine/Weston console test guest.
set -eu
export PATH=/usr/sbin:/usr/bin:/sbin:/bin
/bin/busybox --install -s
mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t devtmpfs devtmpfs /dev
exec >/dev/ttyS0 2>&1
mount -t tmpfs tmpfs /run
mkdir -p /dev/pts /run/desktop
mount -t devpts devpts /dev/pts
chmod 700 /run/desktop

modprobe virtio_gpu
modprobe evdev
modprobe psmouse
case " $(cat /proc/cmdline) " in
    *" nexaui.work=rw "*) work=rw ;;
    *" nexaui.work=ro "*) work=ro ;;
    *) work= ;;
esac
if [ -n "$work" ]; then
    modprobe virtio_blk
fi
udevd --daemon
udevadm trigger --action=add
if ! udevadm settle --timeout=30; then
    printf '%s\n' 'NEXAUI_STARTUP_FAILED udev settlement timed out'
    exit 1
fi
if [ -n "$work" ]; then
    mkdir -p /mnt/work
    if [ "$work" = ro ]; then
        mount -t ext4 -o ro,noload,nodev,nosuid,noexec /dev/vda /mnt/work
        if printf '%s\n' 'NexaUI persisted guest data' | cmp -s - /mnt/work/state.txt; then
            printf '%s\n' 'NEXAUI_STORAGE restored' >/dev/ttyS0
        elif [ ! -e /mnt/work/state.txt ]; then
            printf '%s\n' 'NEXAUI_STORAGE baseline' >/dev/ttyS0
        else
            printf '%s\n' 'NEXAUI_STORAGE mismatch' >/dev/ttyS0
        fi
        umount /mnt/work
    else
        mount -t ext4 -o rw,nodev,nosuid,noexec /dev/vda /mnt/work
        [ ! -e /mnt/work/state.txt ]
        : >/run/work-writable
        printf '%s\n' 'NEXAUI_STORAGE fresh' >/dev/ttyS0
    fi
fi
case " $(cat /proc/cmdline) " in
    *" nexaui.export=write "*|*" nexaui.export=revocable "*|*" nexaui.export=grantable "*)
        modprobe 9pnet_virtio
        modprobe 9p
        mkdir -p /mnt/export
        case " $(cat /proc/cmdline) " in
            *" nexaui.export=grantable "*) : >/run/grantable-export ;;
            *)
                mount -t 9p -o trans=virtio,version=9p2000.L,nodev,nosuid,noexec nexaui-export /mnt/export
                : >/run/file-export
                printf '%s\n' 'NEXAUI_FILE_SHARE mounted' >/dev/ttyS0
                ;;
        esac
        ;;
esac
case " $(cat /proc/cmdline) " in
    *" nexaui.export=revocable "*|*" nexaui.export=grantable "*) : >/run/revocable-export ;;
esac
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
case " $(cat /proc/cmdline) " in
    *" nexaui.clipboard=on "*)
        command -v wl-copy
        command -v wl-paste
        stty -F /dev/ttyS1 raw -echo clocal 115200
        (
            umask 077
            exec 4<>/dev/ttyS1
            printf '%s\n' NEXAUI_CLIPBOARD_READ_READY >/dev/ttyS0
            printf '%s\n' NEXAUI_CLIPBOARD_READY >/dev/ttyS0
            paste_clipboard() (
                set -o pipefail
                timeout "$1" wl-paste --no-newline --type 'text/plain;charset=utf-8' 2>/dev/null |
                    head -c 193 >/run/clipboard-out
            )
            while IFS=' ' read -r ticket expected encoded <&4; do
                if [ "$ticket" = Q ]; then
                    ticket=$expected
                    case "$ticket" in ''|*[!0-9]*) continue ;; esac
                    if [ -z "$encoded" ] && paste_clipboard 2 &&
                        [ -s /run/clipboard-out ] && [ "$(wc -c </run/clipboard-out)" -le 192 ]; then
                        digest=$(sha256sum /run/clipboard-out | cut -d ' ' -f 1)
                        data=$(base64 </run/clipboard-out | tr -d '\n')
                        printf 'D %s %s %s\n' "$ticket" "$digest" "$data" >&4
                    else
                        printf 'R %s\n' "$ticket" >&4
                    fi
                    rm -f /run/clipboard-out
                    continue
                fi
                case "$ticket" in ''|*[!0-9]*) continue ;; esac
                status=R
                if printf '%s' "$encoded" | base64 -d >/run/clipboard-in &&
                    [ "$(wc -c </run/clipboard-in)" -le 192 ] &&
                    [ -s /run/clipboard-in ] &&
                    [ "$(sha256sum /run/clipboard-in | cut -d ' ' -f 1)" = "$expected" ]; then
                    status=U
                    if timeout 2 wl-copy --type 'text/plain;charset=utf-8' </run/clipboard-in >/dev/null 2>&1 &&
                        paste_clipboard 1 && cmp -s /run/clipboard-in /run/clipboard-out; then
                        status=A
                    fi
                fi
                rm -f /run/clipboard-in /run/clipboard-out
                printf '%s %s\n' "$status" "$ticket" >&4
            done
        ) &
        : >/run/clipboard-enabled
        ;;
esac
export TERM=xterm SHELL=/bin/sh
cat >/run/console-probe <<'EOF'
#!/bin/sh
printf 'NexaUI console test\r\n'
printf '%s\n' NEXAUI_DESKTOP_READY >/dev/ttyS0
paint() {
    printf '\033[%sm\033[2J\033[H' "$1"
    for row in 1 2 3 4 5 6; do
        printf '%-60s\r\n' "$2"
    done
    printf '\033[0m'
}
while IFS= read -r line; do
    if [ "$line" = nexa ]; then
        if [ -f /run/file-export ]; then
            printf '%s\n' 'NexaUI guest export' >/mnt/export/guest-export.txt || exit 1
            sync
            printf '%s\n' 'NEXAUI_FILE_SHARE saved' >/dev/ttyS0
        fi
        if [ -f /run/work-writable ]; then
            printf '%s\n' 'NexaUI persisted guest data' >/mnt/work/state.txt || exit 1
            sync
            umount /mnt/work || exit 1
            rm /run/work-writable
            printf '%s\n' 'NEXAUI_STORAGE saved' >/dev/ttyS0
        fi
        sleep .4
        paint 42 'Guest keyboard input received'
        printf '%s\n' NEXAUI_DESKTOP_INPUT >/dev/ttyS0
    elif [ -f /run/clipboard-enabled ] && [ "$line" = clipcheck ]; then
        printf '%s\n' 'Grüße aus NexaUI' 'Zweite Zeile' >/run/clipboard-expected
        timeout 3 wl-paste --no-newline --type 'text/plain;charset=utf-8' 2>/dev/null |
            head -c 193 >/run/clipboard-observed
        if cmp -s /run/clipboard-expected /run/clipboard-observed; then
            printf '%s\n' NEXAUI_CLIPBOARD_INDEPENDENT_OK >/dev/ttyS0
            paint 44 'Guest clipboard matches the approved text'
        else
            printf '%s\n' NEXAUI_CLIPBOARD_INDEPENDENT_FAILED >/dev/ttyS0
            paint 41 'Guest clipboard differs from the approved text'
        fi
        rm -f /run/clipboard-expected /run/clipboard-observed
    elif [ -f /run/clipboard-enabled ] && { [ "$line" = clipoffer ] || [ "$line" = clipbad ]; }; then
        if [ "$line" = clipoffer ]; then
            printf '%s\n' 'Grüße aus dem Gast' 'Zweite Gastzeile' >/run/clipboard-test-offer
            marker=NEXAUI_CLIPBOARD_GUEST_OFFER
        else
            printf 'Unzulässiger\033Text\n' >/run/clipboard-test-offer
            marker=NEXAUI_CLIPBOARD_GUEST_BAD
        fi
        if timeout 3 wl-copy --type 'text/plain;charset=utf-8' </run/clipboard-test-offer >/dev/null 2>&1; then
            printf '%s\n' "$marker" >/dev/ttyS0
        fi
        rm -f /run/clipboard-test-offer
    elif [ -f /run/grantable-export ] && [ "$line" = attach ]; then
        tries=0
        until timeout 3 mount -t 9p -o trans=virtio,version=9p2000.L,cache=none,nodev,nosuid,noexec nexaui-export /mnt/export; do
            tries=$((tries + 1))
            if [ "$tries" -ge 5 ]; then break; fi
            sleep .2
        done
        if [ "$tries" -lt 5 ]; then
            : >/run/file-export
            printf '%s\n' 'NEXAUI_FILE_SHARE mounted' >/dev/ttyS0
            paint 44 'Guest mounted the host export'
        else
            paint 41 'Guest could not mount the host export'
        fi
    elif [ -f /run/file-export ] && [ "$line" = hold ]; then
        if [ -f /mnt/export/guest-export.txt ]; then
            exec 3<>/mnt/export/guest-export.txt
            if IFS= read -r saved <&3 && [ "$saved" = 'NexaUI guest export' ]; then
                : >/run/export-held
                printf '%s\n' 'NEXAUI_FILE_SHARE held' >/dev/ttyS0
                paint 44 'Guest kept its file handle and mount'
            fi
        fi
    elif [ -f /run/export-held ] && [ "$line" = check ]; then
        if grep -q ' /mnt/export .* - 9p nexaui-export ' /proc/self/mountinfo &&
            [ -L /proc/self/fd/3 ] &&
            ! (printf '%s\n' 'Unexpected write through retained handle' >&3) &&
            ! (printf '%s\n' 'Unexpected new file' >/mnt/export/late.txt); then
            printf '%s\n' 'NEXAUI_FILE_SHARE cutoff' >/dev/ttyS0
            paint 42 'Guest writes denied with the mount held'
        else
            paint 41 'Guest retained access or lost its mount'
        fi
    elif [ -f /run/revocable-export ] && [ "$line" = detach ]; then
        umount /mnt/export || continue
        rm -f /run/file-export
        printf '%s\n' 'NEXAUI_FILE_SHARE unmounted' >/dev/ttyS0
        paint 44 'Guest file mount released'
    elif [ -f /run/revocable-export ] && [ "$line" = probe ]; then
        if timeout 3 mount -t 9p -o trans=virtio,version=9p2000.L nexaui-export /mnt/export; then
            printf '%s\n' 'Unexpected guest write' >>/mnt/export/guest-export.txt
            sync
            printf '%s\n' 'NEXAUI_FILE_SHARE unexpected' >/dev/ttyS0
            paint 41 'Guest file transfer still available'
        else
            printf '%s\n' 'NEXAUI_FILE_SHARE denied' >/dev/ttyS0
            paint 42 'Guest could not reopen the host export'
        fi
    fi
done
EOF
chmod 755 /run/console-probe
weston-terminal --maximized --shell=/run/console-probe &
wait "$desktop"
