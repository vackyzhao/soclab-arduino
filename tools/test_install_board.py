"""Native-provider selection and persistent-runtime install tests; no board access."""
import hashlib
import json
import os
import subprocess
import tempfile
import types
import unittest
import zipfile
from pathlib import Path
from unittest import mock

import install_board


class InstallerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        self.source = self.directory / "source"
        self.sketchbook = self.directory / "Arduino Sketchbook"
        (self.source / "tools").mkdir(parents=True)
        (self.source / "boards.txt").write_text("kr260.name=Test board\n")

    def test_windows_arm64_uses_x64_compatibility_and_rv64_is_distinct(self):
        with mock.patch.object(install_board, "os", types.SimpleNamespace(name="nt")), \
             mock.patch.object(install_board.platform, "machine", return_value="ARM64"):
            self.assertEqual(install_board.host_flavour(), ("win32-x64.zip", "x86_64-mingw32"))
        with mock.patch.object(install_board, "os", types.SimpleNamespace(name="posix")), \
             mock.patch.object(install_board.sys, "platform", "linux"), \
             mock.patch.object(install_board.platform, "machine", return_value="riscv64"):
            self.assertEqual(install_board.host_flavour()[1], "riscv64-linux-gnu")

    def test_riscstar_offline_archive_selection_prefix_and_local_config(self):
        archive = self.directory / "riscstar.zip"
        suffix = ".exe" if os.name == "nt" else ""
        with zipfile.ZipFile(archive, "w") as package:
            for program in ("gcc", "gdb"):
                package.writestr("native/bin/riscv32-none-elf-" + program + suffix, b"native-placeholder")
            package.writestr("native/lib/libgcc.a", b"target-library")
        asset = {"host": "riscv64-linux-gnu", "provider": "riscstar", "version": "15.1-r1",
                 "compilerPrefix": "riscv32-none-elf-", "archiveFileName": archive.name,
                 "url": "https://example.invalid/not-downloaded.zip", "size": str(archive.stat().st_size),
                 "checksum": "SHA-256:" + hashlib.sha256(archive.read_bytes()).hexdigest()}
        (self.source / "tools/toolchain.json").write_text(json.dumps({"version": "15.2.0-1", "systems": [asset]}))
        target = self.sketchbook / "hardware/soclab/neorv32"
        target.mkdir(parents=True)
        (target / "boards.local.txt").write_text("kr260.target=private-host\n")
        (target / "platform.local.txt").write_text("compiler.path=old/\ncompiler.prefix=old-\ncustom.option=keep\n")
        with mock.patch.object(install_board, "source_platform", return_value=self.source), \
             mock.patch.object(install_board, "host_flavour", return_value=("unused", "riscv64-linux-gnu")), \
             mock.patch.object(install_board, "release_tool", side_effect=AssertionError("must not query xPack")), \
             mock.patch.object(install_board, "check_programs") as programs:
            self.assertEqual(install_board.main(["--sketchbook", str(self.sketchbook), "--offline", "--archive", str(archive)]), 0)
        expected = self.sketchbook / "tools/soclab/riscstar-toolchain/15.1-r1"
        self.assertTrue((expected / "lib/libgcc.a").is_file())
        programs.assert_called_once_with(expected, "riscv32-none-elf-")
        local = (target / "platform.local.txt").read_text()
        self.assertIn("compiler.prefix=riscv32-none-elf-", local)
        self.assertIn("custom.option=keep", local)
        self.assertNotIn("compiler.prefix=old-", local)
        self.assertEqual((target / "boards.local.txt").read_text(), "kr260.target=private-host\n")

    def test_python_is_copied_to_persistent_directory_and_selected(self):
        runtime = self.directory / "temporary download" / "python"
        binary = runtime / ("python.exe" if os.name == "nt" else "bin/python3")
        binary.parent.mkdir(parents=True)
        binary.write_bytes(b"native-python-placeholder")
        (runtime / "lib").mkdir()
        (runtime / "lib/site.py").write_text("# bundled standard library\n")
        result = subprocess.CompletedProcess([], 0, "Python 3.12.16\n")
        with mock.patch.object(install_board.subprocess, "run", return_value=result) as run:
            python = install_board.install_python_runtime(runtime, self.sketchbook)
        persistent = self.sketchbook / "tools/soclab/python/3.12.16"
        self.assertTrue(python.is_relative_to(persistent))
        self.assertEqual((persistent / "lib/site.py").read_text(), "# bundled standard library\n")
        self.assertEqual(Path(run.call_args.args[0][0]), python)
        target = install_board.install_platform(self.source, self.sketchbook, self.directory / "toolchain",
                                               "riscv-none-elf-", python)
        self.assertIn("soclab.python=" + python.as_posix(), (target / "platform.local.txt").read_text())
        self.assertNotIn("temporary download", (target / "platform.local.txt").read_text())

    def test_runtime_without_an_interpreter_does_not_install(self):
        with self.assertRaisesRegex(RuntimeError, "no host interpreter"):
            install_board.install_python_runtime(self.directory / "missing-runtime", self.sketchbook)
        self.assertFalse(self.sketchbook.exists())

    def test_other_hosts_keep_online_latest_selection(self):
        asset = {"host": "x86_64-linux-gnu", "archiveFileName": "unused", "version": "15.2.0-1"}
        (self.source / "tools/toolchain.json").write_text(json.dumps({"version": "15.2.0-1", "systems": [asset]}))
        with mock.patch.object(install_board, "source_platform", return_value=self.source), \
             mock.patch.object(install_board, "host_flavour", return_value=("linux-x64.tar.gz", "x86_64-linux-gnu")), \
             mock.patch.object(install_board, "release_tool", return_value=("16.0-test", asset)) as release, \
             mock.patch.object(install_board, "valid_toolchain", return_value=True), \
             mock.patch.object(install_board, "check_programs"):
            self.assertEqual(install_board.main(["--sketchbook", str(self.sketchbook)]), 0)
        release.assert_called_once_with(None, "linux-x64.tar.gz", "x86_64-linux-gnu")


if __name__ == "__main__":
    unittest.main()
