"""Publication boundary regressions; no AROS toolchain required."""
import importlib.util
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile
import unittest
import zipfile
import io

SCRIPT = Path(__file__).resolve().parents[1] / 'scripts/check-publication.py'
spec = importlib.util.spec_from_file_location('publication', SCRIPT)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class PublicationTests(unittest.TestCase):
    def test_local_files_and_private_directories(self):
        for name in ['AGENTS.md', 'sub/claude.MD', 'project.json', 'docs/evidence/run/log',
                     'work/build/file', 'applets/org.aros.weather/purchased/sheet.png']:
            with self.subTest(name=name):
                self.assertTrue(module.private_path(name))

    def test_public_files(self):
        for name in ['README.md', 'local.env.example', 'docs/applets.md',
                     'patches/aros-mainline/status.json', 'src/pak.c']:
            self.assertFalse(module.private_path(name))

    def test_download_archive_layouts(self):
        with tempfile.TemporaryDirectory() as directory:
            for kind in ['zip', 'tar.gz']:
                for private in [False, True]:
                    path = Path(directory) / ('source.' + kind)
                    names = ['project-1/src/main.c', 'project-1/README.md']
                    if private:
                        names.append('project-1/AGENTS.md')
                    if kind == 'zip':
                        with zipfile.ZipFile(path, 'w') as archive:
                            for name in names:
                                archive.writestr(name, 'fixture')
                    else:
                        with tarfile.open(path, 'w:gz') as archive:
                            for name in names:
                                entry = tarfile.TarInfo(name)
                                entry.size = 7
                                archive.addfile(entry, io.BytesIO(b'fixture'))
                    result = subprocess.run([sys.executable, str(SCRIPT), '--archive', str(path)],
                                            capture_output=True)
                    self.assertEqual(result.returncode, int(private), result.stderr)


if __name__ == '__main__':
    unittest.main()
