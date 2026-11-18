"""Run the content fixture on its own Xvfb, Weston and AT-SPI bus."""
import argparse
import hashlib
import os
from pathlib import Path
import shutil
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


def run_browser(command, env, engine):
    with tempfile.TemporaryDirectory(prefix='nexaui-browser-') as directory:
        address = Path(directory) / 'engine.sock'
        env['NEXAUI_CONTENT_TEST_BROWSER_SOCKET'] = str(address)
        service = subprocess.Popen([sys.executable, str(Path(__file__).with_name('browser_fixture.py')),
                                    str(engine.resolve(strict=True)), str(address)], env=env)
        try:
            deadline = time.monotonic() + 8
            while not address.exists():
                if service.poll() is not None or time.monotonic() >= deadline:
                    raise RuntimeError('Chromium frame service did not start')
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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, required=True)
    parser.add_argument('--font', type=Path, required=True)
    parser.add_argument('--registry', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    parser.add_argument('--case', choices=('full', 'png', 'pdf', 'input', 'browser', 'pending-close', 'rejected-close'), default='full')
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
    parser.add_argument('--clipboard-reader', type=Path, help='Independent wl-paste executable for PDF copy cases')
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
    if args.browser_input and (not args.browser_engine or not os.environ.get('NEXAUI_WESTON_LIBEXEC')):
        parser.error('--browser-input requires the browser case and Weston simple-IME')
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
    command = ['xvfb-run', '-a', '-s', '-screen 0 1600x1200x24', sys.executable,
               str(repo / 'tests/run_weston_native.py'), '--work-dir', str(args.work_dir),
               '--timeout', '60', '--', sys.executable, str(repo / 'tests/run_atspi_native.py'),
               str(args.registry), str(args.build_dir / 'content_atspi_peer'),
               str(args.build_dir / 'content_native_host')]
    if args.pdf_compose or args.browser_input:
        setxkbmap = os.environ.get('NEXAUI_SETXKBMAP', 'setxkbmap')
        command[4:4] = ['sh', '-ec', '"$1" -layout us -option "" -option compose:ralt; shift; exec "$@"',
                       'content-compose', setxkbmap]
        env.update(NEXAUI_WESTON_INPUT_METHOD='weston-simple-im', XKB_DEFAULT_OPTIONS='compose:ralt')
        flag='NEXAUI_CONTENT_PDF_COMPOSE' if args.pdf_compose else 'NEXAUI_CONTENT_TEST_BROWSER_INPUT'
        env[flag]='1'
        if args.browser_input:
            env['NEXAUI_CONTENT_TEST_BROWSER_WIRE']=str(args.work_dir.resolve() / 'browser-ime.wire')
    if args.browser_engine:
        return run_browser(command, env, args.browser_engine)
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
