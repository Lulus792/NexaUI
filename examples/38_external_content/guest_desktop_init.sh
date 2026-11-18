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
case " $(cat /proc/cmdline) " in
    *" nexaui.usb=probe "*)
        if ! modprobe xhci_pci || ! modprobe usb_storage || ! modprobe sd_mod; then
            printf '%s\n' 'NEXAUI_STARTUP_FAILED USB test drive setup'
            exit 1
        fi
        : >/run/usb-enabled
        printf '%s\n' NEXAUI_USB_READY
        ;;
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
case " $(cat /proc/cmdline) " in
    *" nexaui.microphone=probe "*)
        if ! command -v arecord >/dev/null || ! modprobe snd_hda_codec_generic || ! modprobe snd_hda_intel ||
            ! timeout 4 sh -c 'until [ -c /dev/snd/pcmC0D0c ]; do sleep .1; done' ||
            ! amixer -q -c 0 sset Capture 75% cap; then
            printf '%s\n' 'NEXAUI_STARTUP_FAILED synthetic microphone setup'
            exit 1
        fi
        head -c 192000 /dev/zero >/run/microphone-silence.raw
        : >/run/microphone-enabled
        printf '%s\n' NEXAUI_MICROPHONE_READY
        ;;
esac
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
    *" nexaui.network=probe "*)
        modprobe virtio_net
        : >/run/network-enabled
        ;;
esac
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
network_interface() {
    for path in /sys/class/net/*; do
        if [ "$(cat "$path/address")" = 52:54:00:4e:58:01 ]; then
            printf '%s\n' "${path##*/}"
            return
        fi
    done
    return 1
}
network_new_reply() {
    printf 'first\n' | timeout 2 nc -w 1 10.0.2.100 8080 2>/dev/null
}
network_read() {
    timeout 2 sh -c 'IFS= read -r reply <&5 && [ "$reply" = "$1" ]' reply "$1"
}
usb_target() {
    for path in /sys/bus/usb/devices/*; do
        if [ -f "$path/serial" ] && [ "$(cat "$path/serial")" = NexaUI-TestDisk ]; then
            readlink -f "$path"
            return
        fi
    done
    return 1
}
usb_block() {
    target=$(usb_target) || return 1
    for path in /sys/class/block/sd*; do
        case "$(readlink -f "$path")" in
            "$target"/*) printf '/dev/%s\n' "${path##*/}"; return ;;
        esac
    done
    return 1
}
microphone_window() {
    dd if="$1" bs=192000 skip="$2" count=1 of=/run/microphone-window.raw 2>/dev/null || return 1
    [ "$(stat -c %s /run/microphone-window.raw)" -eq 192000 ] || return 1
    if [ "$3" != active ]; then
        if ! cmp -s /run/microphone-silence.raw /run/microphone-window.raw; then return 1; fi
        printf 'NEXAUI_MICROPHONE_WINDOW %s count=96000 nonzero=0 positive=0 negative=0\n' "$3" >/dev/ttyS0
        return
    fi
    {
        for slice in 0 12 24 36 48 60 72 84; do
            dd if=/run/microphone-window.raw bs=1920 skip="$slice" count=1 2>/dev/null || break
        done
    } | od -An -v -t d2 |
        awk '
            { for (i = 1; i <= NF; ++i) {
                v = $i + 0; ++count
                if (v != 0) ++nonzero
                if (v > 500) ++positive
                if (v < -500) ++negative
            } }
            END {
                printf "NEXAUI_MICROPHONE_WINDOW active count=%d nonzero=%d positive=%d negative=%d\n",
                    count, nonzero, positive, negative
                exit !(count == 7680 && positive > 1500 && negative > 1500)
            }' >/dev/ttyS0
}
microphone_held_window() {
    timeout 12 sh -c '
        while [ ! -f /run/microphone-held.raw ] || [ "$(stat -c %s /run/microphone-held.raw)" -lt "$2" ]; do
            kill -0 "$1" || exit 1
            sleep .1
        done' capture "$microphone_client" "$((($1+1)*192000))" &&
        kill -0 "$microphone_client" && microphone_window /run/microphone-held.raw "$1" "$2"
}
microphone_client=
usb_disk=
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
    elif [ -f /run/usb-enabled ] && [ "$line" = usboff ]; then
        if ! usb_target >/dev/null; then printf '%s\n' NEXAUI_USB_OFF >/dev/ttyS0; fi
    elif [ -f /run/usb-enabled ] && [ "$line" = usbopen ] && [ -z "$usb_disk" ]; then
        tries=0
        until usb_disk=$(usb_block); do
            tries=$((tries+1))
            [ "$tries" -lt 35 ] || break
            sleep .1
        done
        printf '%s\n' 'NexaUI private USB test disk' >/run/usb-expected
        if [ -b "$usb_disk" ] && [ "$(cat "/sys/class/block/${usb_disk##*/}/ro")" = 1 ] &&
            cmp -n 29 /run/usb-expected "$usb_disk" &&
            ! dd if=/dev/zero of="$usb_disk" bs=512 count=1 2>/dev/null; then
            exec 9<"$usb_disk"
            printf '%s\n' NEXAUI_USB_ACTIVE_RO >/dev/ttyS0
        else
            usb_disk=
        fi
    elif [ -n "$usb_disk" ] && [ "$line" = usbcheck ]; then
        tries=0
        while usb_target >/dev/null || [ -e "$usb_disk" ]; do
            tries=$((tries+1))
            [ "$tries" -lt 35 ] || break
            sleep .1
        done
        # The held descriptor has never read the second marker at sector 1000.
        dd bs=512 skip=1000 count=1 <&9 >/run/usb-held-late 2>/dev/null || :
        dd if="$usb_disk" bs=512 count=1 >/run/usb-fresh-late 2>/dev/null || :
        if ! usb_target >/dev/null && [ ! -e "$usb_disk" ] &&
            [ ! -s /run/usb-held-late ] && [ ! -s /run/usb-fresh-late ]; then
            printf '%s\n' NEXAUI_USB_REVOKED_HELD_AND_FRESH >/dev/ttyS0
        fi
        exec 9<&-
        usb_disk=
        rm -f /run/usb-expected /run/usb-held-late /run/usb-fresh-late
    elif [ -f /run/microphone-enabled ] && [ "$line" = micoff ] && [ ! -f /run/microphone-held.raw ]; then
        printf '%s\n' NEXAUI_MICROPHONE_RECORD_START >/dev/ttyS0
        arecord -q -D hw:0,0 -t raw -f S16_LE -c 2 -r 48000 -d 50 /run/microphone-held.raw </dev/null 2>/dev/ttyS0 &
        microphone_client=$!
        if microphone_held_window 0 off; then
            printf '%s\n' NEXAUI_MICROPHONE_OFF >/dev/ttyS0
        else
            printf 'NEXAUI_MICROPHONE_FAILED off bytes=%s\n' "$(stat -c '%s' /run/microphone-held.raw)" >/dev/ttyS0
        fi
    elif [ -n "$microphone_client" ] && [ "$line" = micon ]; then
        printf '%s\n' NEXAUI_MICROPHONE_RECORD_ACTIVE >/dev/ttyS0
        microphone_offset=$(( $(stat -c %s /run/microphone-held.raw) / 192000 + 1 ))
        if microphone_held_window "$microphone_offset" active; then
            printf '%s\n' NEXAUI_MICROPHONE_HELD_ACTIVE >/dev/ttyS0
        else
            printf 'NEXAUI_MICROPHONE_FAILED active bytes=%s offset=%s\n' \
                "$(stat -c '%s' /run/microphone-held.raw)" "$microphone_offset" >/dev/ttyS0
        fi
    elif [ -n "$microphone_client" ] && [ "$line" = miccheck ]; then
        printf '%s\n' NEXAUI_MICROPHONE_RECORD_CUTOFF >/dev/ttyS0
        microphone_offset=$(( $(stat -c %s /run/microphone-held.raw) / 192000 + 1 ))
        microphone_silent=no
        if microphone_held_window "$microphone_offset" revoked-held; then microphone_silent=yes; fi
        if kill "$microphone_client" 2>/dev/null; then wait "$microphone_client" || :; fi
        if [ "$microphone_silent" = yes ] &&
            timeout 5 arecord -q -D hw:0,0 -t raw -f S16_LE -c 2 -r 48000 -d 1 /run/microphone-fresh.raw </dev/null &&
            microphone_window /run/microphone-fresh.raw 0 revoked-fresh; then
            printf '%s\n' NEXAUI_MICROPHONE_CUTOFF >/dev/ttyS0
        else
            printf '%s\n' 'NEXAUI_MICROPHONE_FAILED cutoff' >/dev/ttyS0
        fi
        microphone_client=
        rm -f /run/microphone-held.raw /run/microphone-fresh.raw /run/microphone-window.raw
    elif [ -f /run/network-enabled ] && [ "$line" = netoff ]; then
        if ! network_interface >/dev/null && [ "$(network_new_reply)" != 'NEXAUI first' ]; then
            printf '%s\n' NEXAUI_NETWORK_OFF >/dev/ttyS0
        fi
    elif [ -f /run/network-enabled ] && [ "$line" = netconnect ]; then
        tries=0
        until interface=$(network_interface); do
            tries=$((tries+1))
            [ "$tries" -lt 30 ] || break
            sleep .1
        done
        if [ -n "$interface" ] && ip link set "$interface" up &&
            ip addr replace 10.0.2.15/24 dev "$interface" &&
            ip route replace default via 10.0.2.2 dev "$interface" &&
            mkfifo /run/network-in /run/network-out; then
            exec 4<>/run/network-in 5<>/run/network-out
            nc 10.0.2.100 8080 <&4 >&5 2>/dev/null &
            network_client=$!
            printf 'first\n' >&4
            if network_read 'NEXAUI first' && kill -0 "$network_client"; then
                : >/run/network-held
                printf '%s\n' NEXAUI_NETWORK_CONNECTED >/dev/ttyS0
            fi
        fi
    elif [ -f /run/network-held ] && [ "${line#netcanary}" != "$line" ]; then
        port=${line#netcanary}
        case "$port" in ''|*[!0-9]*) continue ;; esac
        if [ "$port" -ge 1024 ] && [ "$port" -le 65535 ] &&
            ! timeout 2 nc -w 1 10.0.2.2 "$port" </dev/null 2>/dev/null; then
            printf '%s\n' NEXAUI_NETWORK_CANARY_BLOCKED >/dev/ttyS0
        fi
    elif [ -f /run/network-held ] && [ "$line" = netcheck ]; then
        printf 'second\n' >&4
        if ! network_read 'NEXAUI second' && [ "$(network_new_reply)" != 'NEXAUI first' ]; then
            printf '%s\n' NEXAUI_NETWORK_CUTOFF >/dev/ttyS0
        fi
        kill "$network_client" 2>/dev/null || :
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
