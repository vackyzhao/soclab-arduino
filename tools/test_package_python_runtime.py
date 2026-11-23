"""Packaging tests use tiny archives; they do not execute an interpreter."""
import io
import tarfile
import tempfile
import unittest
import zipfile
from pathlib import Path

from package_python_runtime import build_runtime


class PackageRuntimeTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.archive = self.root / "python.tar.gz"
        self.wheel = self.root / "pyserial-3.5-py2.py3-none-any.whl"
        with zipfile.ZipFile(self.wheel, "w") as wheel:
            wheel.writestr("serial/__init__.py", "VERSION = '3.5'\n")
            wheel.writestr("pyserial-3.5.dist-info/WHEEL", "Root-Is-Purelib: true\nTag: py2.py3-none-any\n")
            wheel.writestr("pyserial-3.5.dist-info/LICENSE.txt", "pyserial license\n")

    def python(self, windows=False, full=False, extra=None, symlinks=None):
        prefix = "python/install/" if full else "python/"
        files = {prefix + "LICENSE.txt": b"CPython license\n"}
        if windows:
            files.update({prefix + "python.exe": b"runtime", prefix + "Lib/os.py": b"# stdlib"})
        else:
            files.update({prefix + "bin/python3.13": b"runtime", prefix + "lib/python3.13/os.py": b"# stdlib"})
        if extra:
            files.update(extra)
        with tarfile.open(self.archive, "w:gz") as archive:
            for name, data in files.items():
                entry = tarfile.TarInfo(name)
                entry.size = len(data)
                entry.mode = 0o755 if "bin/python" in name else 0o644
                archive.addfile(entry, io.BytesIO(data))
            if not windows:
                entry = tarfile.TarInfo(prefix + "bin/python3")
                entry.type = tarfile.SYMTYPE
                entry.linkname = "python3.13"
                archive.addfile(entry)
            for name, link in (symlinks or {}).items():
                entry = tarfile.TarInfo(name)
                entry.type = tarfile.SYMTYPE
                entry.linkname = link
                archive.addfile(entry)

    def test_linux_runtime_and_wheel_and_notices(self):
        self.python()
        result = build_runtime(self.archive, self.wheel, "x86_64-pc-linux-gnu", self.root / "dist")
        self.assertEqual(result["executable"], "bin/python3")
        self.assertTrue(result["checksum"].startswith("SHA-256:"))
        with tarfile.open(result["output"]) as archive:
            names = archive.getnames()
            self.assertEqual({name.split('/')[0] for name in names}, {"python-runtime"})
            self.assertIn("python-runtime/lib/python3.13/site-packages/serial/__init__.py", names)
            self.assertIn("python-runtime/licenses/cpython/python/LICENSE.txt", names)
            self.assertIn("python-runtime/licenses/pyserial/pyserial-3.5.dist-info/LICENSE.txt", names)
            self.assertTrue(archive.getmember("python-runtime/bin/python3").issym())
            self.assertEqual(archive.getmember("python-runtime/bin/python3.13").mode, 0o755)

    def test_parent_directories_precede_symlink_placeholders(self):
        # PBS terminfo aliases may have no other file under their parent. The
        # Arduino extractor cannot create a symlink placeholder without it.
        self.python(extra={"python/share/terminfo/x/xterm": b"terminal"},
                    symlinks={"python/share/terminfo/1/1178": "../x/xterm"})
        result = build_runtime(self.archive, self.wheel, "x86_64-linux-gnu", self.root / "dist")
        with tarfile.open(result["output"]) as archive:
            directories = {"."}
            for member in archive:
                parent = Path(member.name).parent.as_posix()
                self.assertIn(parent, directories, member.name)
                if member.isdir():
                    directories.add(member.name.rstrip("/"))
            self.assertIn("python-runtime/share/terminfo/1", directories)

    def test_windows_full_runtime(self):
        self.python(windows=True, full=True)
        output = self.root / "win.tar.gz"
        result = build_runtime(self.archive, self.wheel, "x86_64-mingw32", output)
        with tarfile.open(output) as archive:
            self.assertIn("python-runtime/python.exe", archive.getnames())
            self.assertIn("python-runtime/Lib/site-packages/serial/__init__.py", archive.getnames())
            self.assertFalse(any("/install/Lib/" in name for name in archive.getnames()))
        self.assertEqual(result["executable"], "python.exe")

    def test_parent_path_rejected_without_partial_output(self):
        self.python(extra={"python/../../outside": b"bad"})
        with self.assertRaisesRegex(ValueError, "parent path"):
            build_runtime(self.archive, self.wheel, "aarch64-linux-gnu", self.root / "dist")
        self.assertEqual(list((self.root / "dist").glob("*")), [])

    def test_missing_license_rejected(self):
        self.python()
        with zipfile.ZipFile(self.wheel, "w") as wheel:
            wheel.writestr("serial/__init__.py", "")
            wheel.writestr("pyserial-3.5.dist-info/WHEEL", "Root-Is-Purelib: true\nTag: py3-none-any\n")
        with self.assertRaisesRegex(ValueError, "license notices"):
            build_runtime(self.archive, self.wheel, "aarch64-linux-gnu", self.root / "dist")

    def test_nonportable_wheel_rejected(self):
        self.python()
        with zipfile.ZipFile(self.wheel, "w") as wheel:
            wheel.writestr("serial/__init__.py", "")
            wheel.writestr("pyserial-3.5.dist-info/WHEEL", "Root-Is-Purelib: false\nTag: cp313-cp313-win_amd64\n")
        with self.assertRaisesRegex(ValueError, "pure Python"):
            build_runtime(self.archive, self.wheel, "x86_64-pc-linux-gnu", self.root / "dist")

    def test_older_wheel_with_upstream_license_supplement(self):
        self.python()
        with zipfile.ZipFile(self.wheel, "w") as wheel:
            wheel.writestr("serial/__init__.py", "")
            wheel.writestr("pyserial-3.5.dist-info/WHEEL", "Root-Is-Purelib: true\nTag: py3-none-any\n")
        license_path = self.root / "pyserial-LICENSE.txt"
        license_path.write_text("Official pyserial license supplement\n")
        result = build_runtime(self.archive, self.wheel, "x86_64-pc-linux-gnu",
                               self.root / "dist", license_path)
        with tarfile.open(result["output"]) as archive:
            content = archive.extractfile("python-runtime/licenses/pyserial/LICENSE.txt").read()
        self.assertEqual(content, license_path.read_bytes())
        self.assertEqual(result["pyserialNotices"], 1)


if __name__ == "__main__":
    unittest.main()
