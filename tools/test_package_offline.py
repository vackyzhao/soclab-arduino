"""Exercise actual offline ZIP contents and installer dispatch without hardware."""
import hashlib
import json
import subprocess
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path

from package_offline import build_bundle, launchers


class OfflinePackageTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / "source"
        self.cache = Path(self.temp.name) / "cache"
        self.output = Path(self.temp.name) / "output"
        self.cache.mkdir()
        self.platform = self.root / "hardware/soclab/neorv32"
        (self.platform / "tools").mkdir(parents=True)
        (self.platform / "platform.txt").write_text("version=0.3.0\n")
        (self.platform / "boards.txt").write_text("kr260.name=Test board\n")
        (self.platform / "boards.local.txt").write_text("private=must-not-be-packaged\n")
        self.archive = self.cache / "official-linux-x64.tar.gz"
        self.archive.write_bytes(b"test-native-toolchain-archive")
        asset = {"host": "x86_64-linux-gnu", "archiveFileName": self.archive.name,
                 "size": str(self.archive.stat().st_size),
                 "checksum": "SHA-256:" + hashlib.sha256(self.archive.read_bytes()).hexdigest()}
        (self.platform / "tools/toolchain.json").write_text(json.dumps({"version": "15.2.0-1", "systems": [asset]}))
        (self.root / "README.md").write_text("Test project\n")
        (self.root / "tools").mkdir()
        # This installed-script stub verifies the generated entrypoint supplies
        # local archive/version/offline arguments and forwards a sketchbook path.
        (self.root / "tools/install_board.py").write_text(
            'import argparse\n'
            'def host_flavour(): return "linux-x64.tar.gz", "x86_64-linux-gnu"\n'
            'def main(argv):\n'
            ' p=argparse.ArgumentParser()\n'
            ' p.add_argument("--offline",action="store_true")\n'
            ' p.add_argument("--archive")\n'
            ' p.add_argument("--version")\n'
            ' p.add_argument("--sketchbook")\n'
            ' p.add_argument("--python-runtime")\n'
            ' a=p.parse_args(argv)\n'
            ' assert a.offline and a.version=="15.2.0-1"\n'
            ' assert a.archive.endswith("official-linux-x64.tar.gz")\n'
            ' assert a.sketchbook=="/tmp/School Arduino"\n'
            ' print("offline dispatch OK")\n'
            ' return 0\n')

    def test_complete_zip_and_offline_dispatch(self):
        target = build_bundle(self.root, self.cache, self.output, ["x86_64-linux-gnu"])
        with zipfile.ZipFile(target) as package:
            names = package.namelist()
            self.assertFalse(any(name.endswith(".local.txt") for name in names))
            tool_name = next(n for n in names if "/toolchains/" in n)
            self.assertEqual(package.read(tool_name), self.archive.read_bytes())
            shell_name = next(n for n in names if n.endswith("/install.sh"))
            self.assertTrue(package.getinfo(shell_name).external_attr >> 16 & 0o111)
            extracted = Path(self.temp.name) / "extracted"
            package.extractall(extracted)
        entry = next(extracted.glob("*/install.py"))
        result = subprocess.run([sys.executable, str(entry), "--sketchbook", "/tmp/School Arduino"],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("offline dispatch OK", result.stdout)

    def test_runtime_interpreter_is_bundled_and_selected(self):
        runtime = Path(self.temp.name) / "runtime"
        (runtime / "linux-x64/bin").mkdir(parents=True)
        (runtime / "linux-x64/bin/python3").write_bytes(b"native-python")
        pip_build = "linux-x64/lib/python3.12/site-packages/pip/_internal/operations/build/__init__.py"
        module = runtime / pip_build
        module.parent.mkdir(parents=True)
        module.write_bytes(b"# pip build module must survive packaging\n")
        ignored_build = self.platform / "build/intermediate.o"
        ignored_build.parent.mkdir()
        ignored_build.write_bytes(b"not distributable source")
        (runtime / "manifest.json").write_text(json.dumps({"systems": [
            {"host": "x86_64-linux-gnu", "python": "linux-x64/bin/python3"}]}))
        target = build_bundle(self.root, self.cache, self.output, ["x86_64-linux-gnu"], runtime)
        with zipfile.ZipFile(target) as package:
            self.assertTrue(any(n.endswith("/runtimes/linux-x64/bin/python3") for n in package.namelist()))
            module_entry = next(n for n in package.namelist() if n.endswith("/runtimes/" + pip_build))
            self.assertEqual(package.read(module_entry), module.read_bytes())
            self.assertFalse(any(n.endswith("/build/intermediate.o") for n in package.namelist()))
            shell = package.read(next(n for n in package.namelist() if n.endswith("/install.sh"))).decode()
            self.assertIn('python="$root/runtimes/linux-x64/bin/python3"', shell)
            manifest = json.loads(package.read(next(n for n in package.namelist() if n.endswith("/offline-manifest.json"))))
            self.assertEqual(manifest["systems"][0]["pythonRuntime"], "runtimes/linux-x64")

    def test_missing_or_changed_asset_is_not_packaged(self):
        self.archive.write_bytes(b"partial")
        with self.assertRaisesRegex(RuntimeError, "Incomplete official"):
            build_bundle(self.root, self.cache, self.output, ["x86_64-linux-gnu"])
        self.archive.unlink()
        with self.assertRaisesRegex(RuntimeError, "Cache is missing"):
            build_bundle(self.root, self.cache, self.output, ["x86_64-linux-gnu"])
        self.assertFalse(self.output.exists())

    def test_unsupported_host_is_explicit(self):
        with self.assertRaisesRegex(RuntimeError, "No official toolchain"):
            build_bundle(self.root, self.cache, self.output, ["riscv64-linux-gnu"])

    def test_launcher_paths_with_spaces(self):
        shell, windows = launchers({"x86_64-mingw32": "Windows Tools/python.exe"})
        self.assertIn('"%~dp0runtimes\\Windows Tools\\python.exe"', windows)
        self.assertIn('exec "$python" "$root/install.py" "$@"', shell)


if __name__ == "__main__":
    unittest.main()
