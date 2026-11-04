"""Drag from an independent Wayland source to NexaUI on the private Weston seat."""
import os
import queue
import re
import subprocess
import sys
import threading
import time


def main():
    if len(sys.argv) != 4:
        return 2
    if os.environ.get('NEXAUI_TEST_PRIVATE_WAYLAND') != '1' or os.environ.get('WAYLAND_DISPLAY') != 'nexaui-test':
        return 77
    source_path, target_path, xdotool = sys.argv[1:]
    events = queue.Queue()
    records = []
    processes = []

    def start(path):
        process = subprocess.Popen([path], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1)
        processes.append(process)
        def collect():
            for line in process.stdout:
                value = line.strip()
                records.append(value)
                events.put(value)
        threading.Thread(target=collect, daemon=True).start()
        return process

    def expect(value, seconds):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            try:
                line = events.get(timeout=max(.01, deadline-time.monotonic()))
            except queue.Empty:
                break
            if line == value:
                return
        raise RuntimeError('Missing '+value)

    def move(window, x, y):
        subprocess.run([xdotool, 'mousemove', '--window', window, str(x), str(y)], check=True)

    pressed = False
    try:
        source = start(source_path)
        expect('SOURCE_READY', 10)
        target = start(target_path)
        expect('TARGET_READY', 10)
        with open(os.environ['NEXAUI_TEST_WESTON_LOG'], encoding='utf-8', errors='replace') as log:
            windows = re.findall(r'x11 output .*?window id (\d+)', log.read())
        if len(windows) != 1:
            raise RuntimeError('Private Weston X11 window not found')
        window = windows[0]
        subprocess.run([xdotool, 'windowfocus', window], check=True)
        time.sleep(.4)
        origin = source_point = None
        targets = []
        for y in range(50, 1150, 75):
            for x in range(50, 1550, 75):
                while not events.empty():
                    events.get_nowait()
                move(window, x, y)
                try:
                    line = events.get(timeout=.06)
                except queue.Empty:
                    continue
                match = re.fullmatch(r'(SOURCE|TARGET)_POINTER ([-\d]+) ([-\d]+)', line)
                if not match:
                    continue
                local_x, local_y = int(match[2]), int(match[3])
                if match[1] == 'SOURCE':
                    origin = (x-local_x, y-local_y)
                    source_point = (x, y)
                else:
                    targets.append((x, y))
                if origin and source_point:
                    outside = [p for p in targets if not (origin[0]-20 <= p[0] <= origin[0]+240 and origin[1]-20 <= p[1] <= origin[1]+170)]
                    if outside:
                        target_point = outside[0]
                        break
            else:
                continue
            break
        else:
            raise RuntimeError('Could not locate both non-overlapping Wayland client surfaces')
        move(window, *source_point)
        time.sleep(.15)
        subprocess.run([xdotool, 'mousedown', '1'], check=True)
        pressed = True
        expect('SOURCE_STARTED', 5)
        move(window, *target_point)
        expect('SOURCE_ACTION 1', 5)
        subprocess.run([xdotool, 'mouseup', '1'], check=True)
        pressed = False
        expect('SOURCE_FINISHED', 8)
        if source.wait(timeout=5) != 0 or target.wait(timeout=5) != 0:
            raise RuntimeError('A drag client exited unsuccessfully')
        if not any(line.startswith('TARGET_DROP /tmp/nexaui drop.txt ') for line in records):
            raise RuntimeError('Target did not report the transferred local file')
        print('Independent Wayland source completed copy and NexaUI received the file.')
        return 0
    except (OSError, RuntimeError, subprocess.SubprocessError) as exc:
        print(str(exc), *records[-30:], sep='\n', file=sys.stderr)
        return 1
    finally:
        if pressed:
            subprocess.run([xdotool, 'mouseup', '1'], check=False)
        for process in processes:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()


if __name__ == '__main__':
    sys.exit(main())
