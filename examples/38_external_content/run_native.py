"""Run the content fixture on its own Xvfb, Weston and AT-SPI bus."""
import argparse
import hashlib
import os
from pathlib import Path
import shutil
import stat
import subprocess
import struct
import sys
import tempfile
import time
import zlib


def write_png_fixtures(directory, viewer=False):
    def chunk(tag, data):
        return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data))

    def encode(width, height, pixel):
        rows = b''.join(b'\0' + b''.join(bytes(pixel(x, y)) for x in range(width)) for y in range(height))
        return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0))
                + chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b''))

    width, height = 112, 52
    colors = ((211, 79, 31, 255), (23, 83, 197, 255), (127, 127, 127, 255),
              (71, 163, 31, 255), (191, 63, 137, 255))

    def portrait(x, y):
        if 192 <= y < 224:
            return (255, 206, 86, 255) if y % 2 else (223, 36, 55, 255)
        if 150 <= x < 202 and 80 <= y < 116:
            return (255, 255, 255, 128)
        return colors[1] if y < 192 else colors[3]

    portrait_bytes = encode(208, 416, portrait) if viewer else None
    for kind, color in enumerate(colors):
        if kind == 2:
            image = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0))
        elif viewer and kind in (3, 4):
            image = portrait_bytes
        else:
            image = encode(width, height,
                           lambda x, y: color if kind != 3 or (56 <= x < 84 and 26 <= y < 39) else colors[1])
        (directory / f'{kind}.png').write_bytes(image)
        print(f'PNG_FIXTURE {kind} sha256={hashlib.sha256(image).hexdigest()}', flush=True)


def write_png_gallery(directory, jpeg_fixture=None):
    write_png_fixtures(directory)
    names = ('Meer.png', 'Defekt.png', 'Wald.png', 'Abend.png', 'Morgen.png', 'Himmel.png', 'Küste.png')
    for name, kind in zip(names, (1, 2, 3, 4, 0, 1, 0)):
        shutil.copyfile(directory / f'{kind}.png', directory / name)
    if jpeg_fixture:
        names = (names[0], 'Defekt.jpg', *names[2:-1], 'Küste.jpg')
        photo = directory / names[-1]
        subprocess.run([str(jpeg_fixture), str(photo)], check=True, timeout=15)
        (directory / names[1]).write_bytes(photo.read_bytes()[:-2])
    (directory / 'gallery.txt').write_text('\n'.join(names),encoding='utf-8')


def write_pdf_fixtures(directory, include_links=False, thumbnails=False, properties=False, detail=False):
    def document(pages, properties=None):
        annotated = include_links and len(pages) == 2
        kids = ' '.join(f'{3+2*i} 0 R' for i in range(len(pages)))
        font = 3 + 2 * len(pages)
        names = b' /Names << /Dests << /Names [(first-page) [3 0 R /Fit]] >> >>' if annotated else b''
        objects = [b'<< /Type /Catalog /Pages 2 0 R' + names + b' >>',
                   f'<< /Type /Pages /Kids [{kids}] /Count {len(pages)} >>'.encode()]
        for i, drawing in enumerate(pages):
            annots = ((' /Annots [8 0 R 10 0 R 11 0 R 12 0 R 13 0 R]' if i == 0 else ' /Annots [9 0 R]')
                      if annotated else '')
            objects.append(
                f'<< /Type /Page /Parent 2 0 R /MediaBox [0 0 112 52] '
                f'/Resources << /Font << /F1 {font} 0 R >> >> /Contents {4+2*i} 0 R{annots} >>'.encode())
            objects.append(f'<< /Length {len(drawing)} >>\nstream\n'.encode() + drawing + b'endstream')
        objects.append(b'<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>')
        if annotated:
            base = b'<< /Type /Annot /Subtype /Link /Rect [84 18 105 30] /Border [0 0 0]'
            objects.extend((base + b' /A << /S /GoTo /D [5 0 R /Fit] >> >>',
                            base + b' /Dest (first-page) >>',
                            b'<< /Type /Annot /Subtype /Link /Rect [5 18 22 30] /Border [0 0 0]'
                            b' /A << /S /URI /URI (https://example.invalid/docs?x=1&y=two%20words#part) >> >>',
                            base + b' /A << /S /URI /URI (file:///tmp/example) >> >>',
                            base + b' /A << /S /URI /URI (javascript:ignored) >> >>',
                            base + b' /A << /S /URI /URI (https://user@example.invalid/) >> >>'))
        if properties:
            fields = (f'/{name} <FEFF{value.encode("utf-16-be").hex()}>' for name, value in properties.items())
            objects.append(('<< ' + ' '.join(fields) + ' >>').encode())
        result = bytearray(b'%PDF-1.4\n%\xe2\xe3\xcf\xd3\n')
        offsets = []
        for i, obj in enumerate(objects, 1):
            offsets.append(len(result))
            result.extend(f'{i} 0 obj\n'.encode() + obj + b'\nendobj\n')
        index = len(result)
        result.extend(f'xref\n0 {len(objects)+1}\n0000000000 65535 f \n'.encode())
        for offset in offsets:
            result.extend(f'{offset:010d} 00000 n \n'.encode())
        info = f' /Info {len(objects)} 0 R' if properties else ''
        result.extend(f'trailer\n<< /Size {len(objects)+1} /Root 1 0 R{info} >>\nstartxref\n{index}\n%%EOF\n'.encode())
        return result

    blue = b'0 0 1 rg 0 0 112 52 re f\n'
    # A real PDF text layer in invisible rendering mode, as in OCR documents.
    def text(label):
        return b'BT /F1 8 Tf 3 Tr 8 35 Td (' + label + b') Tj ET\n'

    pages = [blue + text(b'R\\351sum\\351 page one'),
             blue + b'0 1 0 rg 56 13 28 13 re f\n' + text(b'Gr\\374n page two')]
    if detail:
        # At 896 × 416 the red strokes separate into single-pixel columns.
        # The 448 × 208 base blends adjacent red and blue into each pixel.
        stripes = ''.join(f'{40+i/4} 26 .125 8 re\n' for i in range(80))
        pages[1] += b'1 0 0 rg\n' + stripes.encode('ascii') + b'f\n'
    if thumbnails:
        pages.extend(blue + text(f'Preview page {page}'.encode()) for page in range(3, 7))
        pages.append(b'0 1 1 rg 0 0 112 52 re f\n' + text(b'Last preview page'))
    old_properties = {'Title': 'Retired document title'} if properties else None
    authors = ('Zoë Martín, Maja Søndergaard, Anaïs Laurent, Jens Østergård, Léonie Weber, Şirin Kaya, Béla Kovács, '
               "Łucja Nowak, Rui Ferreira, Élodie Dubois, Zoë N'Dour, Lejla Hadžić, Anika Meier, Tereza Nováková")
    current_properties = {'Title': 'Résumé — Übersicht\n2026', 'Author': authors, 'Subject': 'W' * 513} if properties else None
    fixtures = {'old.pdf': document([b'1 0 0 rg 0 0 112 52 re f\n' + text(b'Retired draft')], old_properties),
                'document.pdf': document(pages, current_properties),
                'bad.pdf': b'%PDF-1.4\n1 0 obj\n'}
    for name, data in fixtures.items():
        (directory / name).write_bytes(data)
        print(f'PDF_FIXTURE {name} sha256={hashlib.sha256(data).hexdigest()}', flush=True)


def controlled_pdf_renderer(directory, export=False):
    path = Path(directory) / 'detail-renderer'
    settings = 'held_edges, failed_edge = (1024,), 0' if export else 'held_edges, failed_edge = (672, 896), 1024'
    path.write_text(f'#!{sys.executable}\n{settings}\n' + '''import os, sys, time
from pathlib import Path
edge = int(sys.argv[sys.argv.index('-scale-to')+1])
if edge == failed_edge:
    sys.exit(1)
if edge in held_edges:
    directory = Path(__file__).parent
    (directory / f'detail-{edge}-started').touch()
    release = directory / f'detail-{edge}-release'
    deadline = time.monotonic() + 2.6
    while not release.exists():
        if time.monotonic() >= deadline:
            sys.exit(2)
        time.sleep(.005)
renderer = os.environ['NEXAUI_CONTENT_TEST_DETAIL_RENDERER']
os.execv(renderer, [renderer, *sys.argv[1:]])
''', encoding='utf-8')
    path.chmod(0o700)
    return path


def run_content_service(command, env, address, args, startup_timeout=12, ready=None):
    service = subprocess.Popen(args, env=env)
    try:
        deadline = time.monotonic() + startup_timeout
        while not (ready or address).exists():
            if service.poll() is not None or time.monotonic() >= deadline:
                raise RuntimeError('Content frame service did not start')
            time.sleep(.02)
        result = subprocess.run(command, env=env, timeout=90).returncode
    finally:
        if service.poll() is None:
            service.terminate()
        try:
            service.wait(timeout=10)
        except subprocess.TimeoutExpired:
            service.kill()
            service.wait()
            raise
    return result or service.returncode


def run_browser(command, env, engine, accessible=False):
    with tempfile.TemporaryDirectory(prefix='nexaui-browser-') as directory:
        address = Path(directory) / 'engine.sock'
        env['NEXAUI_CONTENT_TEST_BROWSER_SOCKET'] = str(address)
        args = [sys.executable, str(Path(__file__).with_name('browser_fixture.py')),
                str(engine.resolve(strict=True)), str(address)]
        if accessible:
            args.append('--ax')
            env['NEXAUI_CONTENT_TEST_BROWSER_AX'] = '1'
        return run_content_service(command,env,address,args)


def verify_guest_disk(engine, firmware, boot, initrd, disk, root, reset=False):
    from guest_fixture import Guest

    def digest():
        with disk.open('rb') as image:
            return hashlib.file_digest(image,'sha256').hexdigest()

    check_dir = root/'verify'
    check_dir.mkdir(mode=0o700)
    before = digest()
    guest = Guest(engine, firmware, boot, check_dir, initrd, disk, writable=False)
    try:
        if guest.wait_ready() != 'running':
            raise RuntimeError('Unexpected storage verification state')
        with guest.trace.open('rb') as log:
            serial = log.read(65536).splitlines()
        marker=b'NEXAUI_STORAGE baseline' if reset else b'NEXAUI_STORAGE restored'
        if marker not in serial:
            raise RuntimeError(f'The new guest did not report {marker.decode()}')
        _, state, flags, _, _, capacity, read, written = struct.unpack('>4sBBHQQQQ',guest.resources())
        if state != 1 or flags != 1 or capacity != disk.stat().st_size or not read or written:
            raise RuntimeError('Read-only guest I/O did not match the verification contract')
    finally:
        if guest.close():
            raise RuntimeError('Storage verification guest failed to exit')
    if digest() != before:
        raise RuntimeError('Read-only guest changed disk bytes')
    label='GUEST_RESET_VERIFIED' if reset else 'GUEST_STORAGE_VERIFIED'
    print(f'{label} mode=readonly bytes={capacity} read={read} written={written} sha256={before}',flush=True)


def verify_guest_export(path):
    expected = b'NexaUI guest export\n'
    fd = os.open(path,os.O_RDONLY|os.O_NOFOLLOW|os.O_NONBLOCK)
    with os.fdopen(fd,'rb') as exported:
        info = os.fstat(exported.fileno())
        if not stat.S_ISREG(info.st_mode) or info.st_size!=len(expected) or exported.read(len(expected)+1)!=expected:
            raise RuntimeError('Unexpected guest-written host file')
    return info.st_dev,info.st_ino,hashlib.sha256(expected).hexdigest()


def run_guest(command, env, engine, firmware, kernel=None, initrd=None, storage=False, reset=False, restart=False,
              sharing=False, revoke=False, grant=False, bindfs=None, clipboard_source=None, clipboard_reader=None,
              network=False, microphone=False, usb=False, accessibility=False, extended_pointer=False):
    from guest_fixture import desktop_boot_timeout

    engine = str(engine.resolve(strict=True))
    firmware = str(firmware.resolve(strict=True))
    with tempfile.TemporaryDirectory(prefix='nexaui-guest-',dir='/tmp' if sharing else None) as directory:
        root = Path(directory)
        if kernel:
            boot=kernel.resolve(strict=True)
            initrd=initrd.resolve(strict=True)
            if not boot.is_file() or not initrd.is_file():
                raise ValueError('Guest kernel and initrd must be files')
        else:
            sector,boot = root/'boot.o',root/'boot.img'
            assembler,objcopy=shutil.which('clang'),shutil.which('objcopy')
            if not assembler or not objcopy:
                raise RuntimeError('The boot fixture requires clang and objcopy')
            source = Path(__file__).with_name('guest_boot.S')
            subprocess.run([assembler,'-target','i386-linux-musl','-c',str(source),'-o',str(sector)],check=True,timeout=15)
            subprocess.run([objcopy,'-O','binary','-j','.text',str(sector),str(boot)],check=True,timeout=15)
            code=boot.read_bytes()
            if len(code)!=512 or code[-2:]!=b'\x55\xaa':
                raise RuntimeError('Invalid fixture boot sector')
            with boot.open('ab') as file:
                file.truncate(1474560)
            print(f'GUEST_BOOT sector_sha256={hashlib.sha256(code).hexdigest()}',flush=True)
        address=root/'guest.sock'
        env['NEXAUI_CONTENT_TEST_GUEST_SOCKET']=str(address)
        env.pop('NEXAUI_CONTENT_TEST_FILE_SHARE',None)
        env.pop('NEXAUI_CONTENT_TEST_FILE_REVOKE',None)
        env.pop('NEXAUI_CONTENT_TEST_FILE_GRANT',None)
        env.pop('NEXAUI_CONTENT_TEST_FILE_BINDFS',None)
        env.pop('NEXAUI_CONTENT_TEST_GUEST_CLIPBOARD',None)
        env.pop('NEXAUI_CONTENT_TEST_HOST_WL_COPY',None)
        env.pop('NEXAUI_CONTENT_TEST_GUEST_NETWORK',None)
        if network:
            env['NEXAUI_CONTENT_TEST_GUEST_NETWORK']='1'
        env.pop('NEXAUI_CONTENT_TEST_GUEST_MICROPHONE',None)
        if microphone:
            env['NEXAUI_CONTENT_TEST_GUEST_MICROPHONE']='1'
        env.pop('NEXAUI_CONTENT_TEST_GUEST_USB',None)
        if usb:
            env['NEXAUI_CONTENT_TEST_GUEST_USB']='1'
        env.pop('NEXAUI_CONTENT_TEST_GUEST_AX',None)
        if accessibility:
            env['NEXAUI_CONTENT_TEST_GUEST_AX']='1'
        env.pop('NEXAUI_CONTENT_TEST_GUEST_POINTER',None)
        if extended_pointer:
            env['NEXAUI_CONTENT_TEST_GUEST_POINTER']='1'
        if clipboard_source:
            env['NEXAUI_CONTENT_TEST_GUEST_CLIPBOARD']='1'
            env['NEXAUI_CONTENT_TEST_HOST_WL_COPY']=clipboard_source
            env['NEXAUI_CONTENT_TEST_CLIPBOARD_READER']=clipboard_reader
        if sharing:
            (root/'host-export').mkdir(mode=0o700)
            env['NEXAUI_CONTENT_TEST_FILE_SHARE']='1'
            mode='OFFERED' if grant else 'GRANTED'
            print(f'GUEST_FILE_SHARE_{mode} path={root / "host-export"}',flush=True)
        if revoke or grant:
            env['NEXAUI_CONTENT_TEST_FILE_REVOKE']='1'
        if grant:
            env['NEXAUI_CONTENT_TEST_FILE_GRANT']='1'
        if bindfs:
            env['NEXAUI_CONTENT_TEST_FILE_BINDFS']=str(bindfs.resolve(strict=True))
        args=[sys.executable,str(Path(__file__).with_name('guest_fixture.py')),
              engine,firmware,str(boot),str(address)]
        if initrd:
            args.append(str(initrd))
        disk = root/'work.img'
        if reset and not shutil.which('debugfs'):
            raise RuntimeError('The guest reset fixture requires debugfs to read the retained disk')
        if storage:
            from guest_fixture import WORK_DISK_BYTES, image_digest

            formatter=shutil.which('mke2fs')
            if not formatter:
                raise RuntimeError('The guest storage fixture requires mke2fs')
            with disk.open('xb') as image:
                image.truncate(WORK_DISK_BYTES)
            subprocess.run([formatter,'-q','-F','-t','ext4','-L','nexaui-work','-E',
                            'lazy_itable_init=0,lazy_journal_init=0',str(disk)],check=True,timeout=15)
            checkpoint = root/'baseline.img'
            with disk.open('rb') as image, checkpoint.open('xb') as baseline:
                shutil.copyfileobj(image,baseline)
            checkpoint.chmod(0o400)
            baseline_hash = image_digest(checkpoint)
            print(f'GUEST_RESET_BASELINE sha256={baseline_hash.hex()} capacity={WORK_DISK_BYTES}',flush=True)
            args.extend((str(disk),str(checkpoint)))
            if reset:
                args.append('restart-before-reply' if restart else 'drop-response')
        result=run_content_service(command,env,address,args,startup_timeout=12+(desktop_boot_timeout(accessibility) if initrd else 0),
                                   ready=address.with_suffix('.ready') if restart else None)
        if storage and result==0:
            if image_digest(checkpoint)!=baseline_hash or (image_digest(disk)==baseline_hash)!=reset:
                raise RuntimeError('The retained work and baseline images are inconsistent')
            if reset:
                previous=root/'work-before-reset.img'
                retained=image_digest(previous)
                contents=subprocess.check_output(['debugfs','-R','cat /state.txt',str(previous)],timeout=10)
                if retained==baseline_hash or contents!=b'NexaUI persisted guest data\n':
                    raise RuntimeError('The retained disk is missing the guest-written content')
                print(f'GUEST_RESET_RETAINED sha256={retained.hex()} bytes={len(contents)}',flush=True)
            # The engine inherits the private runtime environment for both boots.
            export = root/'host-export'/'guest-export.txt'
            before = verify_guest_export(export) if sharing else None
            verify_guest_disk(engine,firmware,str(boot),str(initrd),disk,root,reset=reset)
            if sharing:
                if verify_guest_export(export)!=before:
                    raise RuntimeError('The external guest write changed during baseline verification')
                print(f'GUEST_FILE_SHARE_VERIFIED path={export} sha256={before[2]}',flush=True)
        return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, required=True)
    parser.add_argument('--font', type=Path, required=True)
    parser.add_argument('--registry', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    parser.add_argument('--renderer', choices=('software', 'vulkan'), default='software',
                        help='Presenter required for the native content window')
    parser.add_argument('--case', choices=('full', 'png', 'pdf', 'input', 'browser', 'guest', 'guest-desktop','guest-storage','guest-reset',
                                           'pending-close', 'rejected-close'), default='full')
    parser.add_argument('--pdf-renderer', type=Path, help='App-selected pdftoppm executable for --case pdf')
    parser.add_argument('--pdf-text-reader', type=Path, help='pdftotext path, default next to pdftoppm')
    parser.add_argument('--pdf-link-reader', type=Path, help='Optional content_pdf_link_reader path')
    parser.add_argument('--pdf-select', action='store_true', help='Exercise PDF text selection and copy')
    parser.add_argument('--pdf-external', action='store_true', help='Exercise the PDF link preview and local handoff')
    parser.add_argument('--pdf-zoom', action='store_true', help='Exercise the PDF zoom, pan and backing-scale workflow')
    parser.add_argument('--pdf-thumbnails', action='store_true', help='Exercise the seven-page PDF preview workflow')
    parser.add_argument('--pdf-properties', action='store_true', help='Exercise the PDF properties dialog')
    parser.add_argument('--pdf-detail', action='store_true', help='Exercise higher-resolution PDF zoom rendering')
    parser.add_argument('--pdf-export', action='store_true', help='Export the current PDF page viewport to PNG')
    parser.add_argument('--png-viewer', action='store_true', help='View landscape and portrait PNGs, including zoom and pan')
    parser.add_argument('--png-gallery', action='store_true', help='Open the host-owned ordered PNG gallery')
    parser.add_argument('--jpeg-gallery', action='store_true', help='Include an EXIF-oriented JPEG in --png-gallery')
    parser.add_argument('--browser-engine', type=Path, help='Chromium headless shell for the offline browser frame workflow')
    parser.add_argument('--browser-input', action='store_true', help='Exercise native keyboard and Weston Compose in Chromium')
    parser.add_argument('--browser-pointer', action='store_true', help='Exercise native pointer and wheel routing in Chromium')
    parser.add_argument('--browser-accessibility', action='store_true', help='Exercise the browser document subtree through native AT-SPI')
    parser.add_argument('--guest-engine', type=Path, help='QEMU system x86-64 executable for --case guest')
    parser.add_argument('--guest-firmware', type=Path, help='QEMU packaged firmware directory for --case guest')
    parser.add_argument('--guest-kernel', type=Path, help='Linux kernel for --case guest-desktop')
    parser.add_argument('--guest-initrd', type=Path, help='Alpine/Weston initramfs for --case guest-desktop')
    parser.add_argument('--guest-owner-restart', action='store_true', help='Replace the guest owner after a committed reset')
    sharing = parser.add_mutually_exclusive_group()
    sharing.add_argument('--guest-file-share', action='store_true', help='Grant the reset guest one private writable export at launch')
    sharing.add_argument('--guest-file-grant', action='store_true', help='Exercise granting and revoking a private file export in the running guest')
    parser.add_argument('--guest-file-revoke', action='store_true', help='Exercise runtime removal of the private file export')
    parser.add_argument('--guest-file-bindfs', type=Path, help='Bindfs executable for revoking access without guest unmount')
    parser.add_argument('--guest-clipboard', action='store_true', help='Exercise reviewed text transfers between host and guest')
    parser.add_argument('--guest-network', action='store_true', help='Grant and revoke the single private guest TCP test endpoint')
    parser.add_argument('--guest-microphone', action='store_true', help='Grant and revoke the synthetic guest microphone tone')
    parser.add_argument('--guest-usb', action='store_true', help='Attach and detach a read-only emulated guest USB test drive')
    parser.add_argument('--guest-ax', action='store_true', help='Read the selected fullscreen guest through native AT-SPI')
    parser.add_argument('--guest-compose', action='store_true', help='Exercise Unicode composition in the fullscreen GTK guest')
    parser.add_argument('--guest-pointer', action='store_true', help='Exercise the graphical guest wheel and side buttons')
    parser.add_argument('--clipboard-reader', type=Path, help='Independent wl-paste executable for PDF or guest clipboard cases')
    parser.add_argument('--pdf-compose', action='store_true', help='Also exercise Weston simple-IME Compose')
    args = parser.parse_args()
    if args.png_viewer and args.case != 'png':
        parser.error('--png-viewer requires --case png')
    if args.png_gallery and (args.case != 'png' or args.png_viewer):
        parser.error('--png-gallery requires --case png and runs separately from --png-viewer')
    if args.jpeg_gallery and not args.png_gallery:
        parser.error('--jpeg-gallery requires --png-gallery')
    if (args.case == 'browser') != bool(args.browser_engine):
        parser.error('--case browser and --browser-engine must be used together')
    desktop_guest=args.case in ('guest-desktop','guest-storage','guest-reset')
    if args.guest_owner_restart and args.case!='guest-reset':
        parser.error('--guest-owner-restart requires --case guest-reset')
    if (args.guest_file_share or args.guest_file_grant) and args.case!='guest-reset':
        parser.error('guest file sharing requires --case guest-reset')
    if args.guest_file_revoke and not args.guest_file_share:
        parser.error('--guest-file-revoke requires --guest-file-share')
    if args.guest_file_bindfs and not (args.guest_file_grant and shutil.which('fusermount3')):
        parser.error('--guest-file-bindfs requires --guest-file-grant and fusermount3')
    clipboard_source=shutil.which('wl-copy') if args.guest_clipboard else None
    guest_clipboard_reader=(args.clipboard_reader or shutil.which('wl-paste')) if args.guest_clipboard else None
    if args.guest_clipboard and not (desktop_guest and clipboard_source and guest_clipboard_reader):
        parser.error('--guest-clipboard requires a graphical guest and host wl-copy and wl-paste')
    if args.guest_network and not desktop_guest:
        parser.error('--guest-network requires a graphical guest')
    if args.guest_microphone and not desktop_guest:
        parser.error('--guest-microphone requires a graphical guest')
    if args.guest_usb and not desktop_guest:
        parser.error('--guest-usb requires a graphical guest')
    if args.guest_ax and not desktop_guest:
        parser.error('--guest-ax requires a graphical guest')
    if args.guest_compose and not args.guest_ax:
        parser.error('--guest-compose requires --guest-ax')
    if args.guest_pointer and not desktop_guest:
        parser.error('--guest-pointer requires a graphical guest')
    if (desktop_guest or args.case=='guest') != bool(args.guest_engine) or bool(args.guest_engine)!=bool(args.guest_firmware):
        parser.error('guest cases require --guest-engine and --guest-firmware')
    if desktop_guest != bool(args.guest_kernel) or bool(args.guest_kernel)!=bool(args.guest_initrd):
        parser.error('graphical guest cases require --guest-kernel and --guest-initrd')
    if (args.browser_input or args.browser_pointer) and (not args.browser_engine or not os.environ.get('NEXAUI_WESTON_LIBEXEC')):
        parser.error('Browser input workflows require the browser case and Weston simple-IME')
    if args.browser_input and args.browser_pointer:
        parser.error('Run browser pointer and text input workflows separately')
    if args.browser_accessibility and (not args.browser_engine or args.browser_input or args.browser_pointer):
        parser.error('--browser-accessibility is a separate browser workflow')
    if args.case == 'pdf' and not args.pdf_renderer:
        parser.error('--case pdf requires --pdf-renderer')
    if args.pdf_link_reader and args.case != 'pdf':
        parser.error('--pdf-link-reader requires --case pdf')
    if args.pdf_select and args.case != 'pdf':
        parser.error('--pdf-select requires --case pdf')
    if args.pdf_external and (args.case != 'pdf' or not args.pdf_link_reader or args.pdf_select):
        parser.error('--pdf-external requires --case pdf and --pdf-link-reader and runs separately from --pdf-select')
    if args.pdf_zoom and (args.case != 'pdf' or not args.pdf_link_reader or args.pdf_select or args.pdf_external):
        parser.error('--pdf-zoom requires --case pdf and --pdf-link-reader as a separate workflow')
    if args.pdf_thumbnails and (args.case != 'pdf' or args.pdf_link_reader or args.pdf_select or args.pdf_external or
                                args.pdf_zoom or args.pdf_compose or args.pdf_properties or args.pdf_detail or args.pdf_export):
        parser.error('--pdf-thumbnails requires --case pdf as a separate workflow')
    if args.pdf_properties and (args.case != 'pdf' or not args.pdf_link_reader or args.pdf_select or args.pdf_external or
                                args.pdf_zoom or args.pdf_compose or args.pdf_detail):
        parser.error('--pdf-properties requires --case pdf and --pdf-link-reader as a separate workflow')
    if args.pdf_detail and (args.case != 'pdf' or not args.pdf_link_reader or args.pdf_select or args.pdf_external or
                            args.pdf_zoom or args.pdf_compose):
        parser.error('--pdf-detail requires --case pdf and --pdf-link-reader as a separate workflow')
    if args.pdf_export and (args.case != 'pdf' or args.pdf_select or args.pdf_external or args.pdf_zoom or
                            args.pdf_compose or args.pdf_properties or args.pdf_detail):
        parser.error('--pdf-export requires --case pdf as a separate workflow')
    if args.pdf_compose and (args.case != 'pdf' or not os.environ.get('NEXAUI_WESTON_LIBEXEC')):
        parser.error('--pdf-compose requires --case pdf and a private Weston helper directory')
    for key in ('build_dir', 'font', 'registry'):
        setattr(args, key, getattr(args, key).resolve(strict=True))
    args.work_dir = args.work_dir.resolve()
    if args.work_dir.exists():
        parser.error('Use a new private work directory')
    xdotool = shutil.which('xdotool')
    if not xdotool:
        parser.error('xdotool is required for the private Xvfb seat')
    repo = Path(__file__).resolve().parents[2]
    env = dict(os.environ, NEXAUI_PLATFORM='wayland', NEXAUI_TEST_PRIVATE_X11='1',
               NEXAUI_CONTENT_TEST_FONT=str(args.font), NEXAUI_CONTENT_TEST_CASE=args.case,
               NEXAUI_CONTENT_TEST_RENDERER=args.renderer,
               NEXAUI_CONTENT_TEST_XDOTOOL=xdotool,
               NEXAUI_CONTENT_CAPTURE_PREFIX=str(args.work_dir / 'content-'))
    if args.pdf_zoom:
        env['NEXAUI_CONTENT_TEST_PDF_ZOOM'] = '1'
    if args.pdf_thumbnails:
        env['NEXAUI_CONTENT_TEST_PDF_THUMBNAILS'] = '1'
    if args.pdf_properties:
        env['NEXAUI_CONTENT_TEST_PDF_PROPERTIES'] = '1'
    if args.pdf_detail:
        env['NEXAUI_CONTENT_TEST_PDF_DETAIL'] = '1'
    if args.pdf_export:
        env['NEXAUI_CONTENT_TEST_EXPORT_PATH'] = str(args.work_dir / 'exported-page.png')
    if args.png_viewer:
        env['NEXAUI_CONTENT_TEST_PNG_VIEWER'] = '1'
    if args.png_gallery:
        env['NEXAUI_CONTENT_TEST_PNG_GALLERY'] = '1'
    if args.jpeg_gallery:
        env['NEXAUI_CONTENT_TEST_JPEG_GALLERY'] = '1'
    if args.pdf_select or args.pdf_external:
        reader = args.clipboard_reader or shutil.which('wl-paste')
        if not reader:
            parser.error('PDF copy cases need wl-paste or --clipboard-reader')
        env['NEXAUI_CONTENT_TEST_CLIPBOARD_READER'] = str(Path(reader).resolve(strict=True))
        if args.pdf_select:
            env['NEXAUI_CONTENT_TEST_PDF_SELECT'] = '1'
        if args.pdf_external:
            env.update(NEXAUI_CONTENT_TEST_PDF_URL='1',
                       NEXAUI_CONTENT_TEST_URI_LAUNCHER=str(args.build_dir / 'content_atspi_peer'),
                       NEXAUI_CONTENT_TEST_URI_RECEIPT=str(args.work_dir / 'uri-receipt.txt'))
    extended_guest = args.guest_microphone or args.guest_usb or args.guest_ax
    env.pop('NEXAUI_CONTENT_TEST_GUEST_COMPOSE', None)
    if args.guest_compose:
        env['NEXAUI_CONTENT_TEST_GUEST_COMPOSE'] = '1'
    if extended_guest:
        env['NEXAUI_TEST_ATSPI_CLIENT_TIMEOUT'] = '75'
    command = ['xvfb-run', '-a', '-s', '-screen 0 1600x1200x24', sys.executable,
               str(repo / 'tests/run_weston_native.py'), '--work-dir', str(args.work_dir),
               '--timeout', '82' if extended_guest else '60', '--',
               sys.executable, str(repo / 'tests/run_atspi_native.py'),
               str(args.registry), str(args.build_dir / 'content_atspi_peer'),
               str(args.build_dir / 'content_native_host')]
    if args.pdf_compose or args.browser_input or args.browser_pointer:
        setxkbmap = os.environ.get('NEXAUI_SETXKBMAP', 'setxkbmap')
        command[4:4] = ['sh', '-ec', '"$1" -layout us -option "" -option compose:ralt; shift; exec "$@"',
                       'content-compose', setxkbmap]
        env.update(NEXAUI_WESTON_INPUT_METHOD='weston-simple-im', XKB_DEFAULT_OPTIONS='compose:ralt')
        flag='NEXAUI_CONTENT_PDF_COMPOSE' if args.pdf_compose else (
            'NEXAUI_CONTENT_TEST_BROWSER_INPUT' if args.browser_input else 'NEXAUI_CONTENT_TEST_BROWSER_POINTER')
        env[flag]='1'
        if args.browser_input:
            env['NEXAUI_CONTENT_TEST_BROWSER_WIRE']=str(args.work_dir.resolve() / 'browser-ime.wire')
    if args.browser_engine:
        return run_browser(command, env, args.browser_engine,args.browser_accessibility)
    if args.guest_engine:
        return run_guest(command,env,args.guest_engine,args.guest_firmware,args.guest_kernel,args.guest_initrd,
                         storage=args.case in ('guest-storage','guest-reset'),reset=args.case=='guest-reset',
                         restart=args.guest_owner_restart,sharing=args.guest_file_share or args.guest_file_grant,
                         revoke=args.guest_file_revoke,grant=args.guest_file_grant,bindfs=args.guest_file_bindfs,
                         clipboard_source=clipboard_source,
                         clipboard_reader=str(Path(guest_clipboard_reader).resolve(strict=True)) if guest_clipboard_reader else None,
                         network=args.guest_network,microphone=args.guest_microphone,usb=args.guest_usb,
                         accessibility=args.guest_ax,extended_pointer=args.guest_pointer)
    if args.case in ('png', 'pdf'):
        with tempfile.TemporaryDirectory(prefix='nexaui-content-') as directory:
            env['NEXAUI_CONTENT_TEST_ENCODED'] = directory
            if args.case == 'png':
                if args.png_gallery:
                    write_png_gallery(Path(directory), args.build_dir / 'content_jpeg_test' if args.jpeg_gallery else None)
                else:
                    write_png_fixtures(Path(directory),args.png_viewer)
            else:
                renderer = args.pdf_renderer.resolve(strict=True)
                reader = args.pdf_text_reader or renderer.with_name('pdftotext')
                if args.pdf_detail or args.pdf_export:
                    env['NEXAUI_CONTENT_TEST_DETAIL_RENDERER'] = str(renderer)
                    renderer = controlled_pdf_renderer(directory,args.pdf_export)
                env['NEXAUI_CONTENT_TEST_PDF_RENDERER'] = str(renderer)
                env['NEXAUI_CONTENT_TEST_PDF_READER'] = str(reader.resolve(strict=True))
                if args.pdf_link_reader:
                    env['NEXAUI_CONTENT_TEST_PDF_LINK_READER'] = str(args.pdf_link_reader.resolve(strict=True))
                write_pdf_fixtures(Path(directory),bool(args.pdf_link_reader),args.pdf_thumbnails,
                                   args.pdf_properties,args.pdf_detail)
            return subprocess.run(command, env=env, timeout=90).returncode
    return subprocess.run(command, env=env, timeout=90).returncode


if __name__ == '__main__':
    raise SystemExit(main())
