"""End-to-end guards against stale HLSL and damaged embedded DXBC."""
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
import unittest


class EmbeddedShaderChecks(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='nexaui-dxbc-check-')
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        source = Path(__file__).resolve().parents[1]
        (self.root/'tools').mkdir()
        shutil.copyfile(source/'tools/generate_d3d12_shaders.py', self.root/'tools/generate_d3d12_shaders.py')
        self.shaders = self.root/'renderers/d3d12'
        self.shaders.mkdir(parents=True)
        shutil.copytree(source/'renderers/d3d12/shaders', self.shaders/'shaders')
        shutil.copyfile(source/'renderers/d3d12/shader_bytecode.h', self.shaders/'shader_bytecode.h')

    def invoke(self, *arguments):
        return subprocess.run([sys.executable, str(self.root/'tools/generate_d3d12_shaders.py'), *arguments],
                              capture_output=True, text=True, timeout=20)

    def test_unmodified_artifacts(self):
        result = self.invoke('--check-source')
        self.assertEqual(result.returncode, 0, result.stdout+result.stderr)

    def test_authored_source_must_be_regenerated(self):
        path = self.shaders/'shaders/bitmap.hlsl'
        path.write_bytes(path.read_bytes()+b'// stale authored source\n')
        result = self.invoke('--check-source')
        self.assertEqual(result.returncode, 1)
        self.assertIn('integrity mismatch: bitmap.hlsl', result.stderr)

    def test_corrupt_embedded_dxbc_is_rejected(self):
        path = self.shaders/'shader_bytecode.h'
        text = path.read_text(encoding='utf-8')
        self.assertIn('0x44, 0x58, 0x42, 0x43', text)
        path.write_text(text.replace('0x44, 0x58, 0x42, 0x43', '0x45, 0x58, 0x42, 0x43', 1), encoding='utf-8')
        result = self.invoke('--check-source')
        self.assertEqual(result.returncode, 1)
        self.assertIn('Invalid embedded DXBC', result.stderr)

    @unittest.skipUnless(sys.platform == 'win32', 'requires the Windows HLSL compiler')
    def test_compiled_pixel_change_must_not_match_old_bytecode(self):
        path = self.shaders/'shaders/bitmap.hlsl'
        text = path.read_text(encoding='utf-8')
        old = 'return float4(rgb, alpha) / 255.0;'
        self.assertIn(old, text)
        path.write_text(text.replace(old, 'return float4(rgb, alpha) / 255.0 * 0.5;'), encoding='utf-8', newline='\n')
        result = self.invoke('--check')
        self.assertEqual(result.returncode, 1)
        self.assertIn('Embedded D3D12 bytecode differs', result.stderr)


if __name__ == '__main__':
    unittest.main()
