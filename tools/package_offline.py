#!/usr/bin/env python3
"""Package SoCLab and cached native toolchains for installation without network access.

Large upstream archives remain release assets, not files committed to Git.
--runtime-dir optionally supplies complete relocatable Python distributions and
a manifest.json with systems [{host, python}], with python relative to that dir.
"""
import argparse
import json
import re
import stat
import sys
import zipfile
from pathlib import Path, PurePosixPath

from install_board import check_archive

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_HOSTS = ("x86_64-mingw32", "x86_64-linux-gnu", "aarch64-linux-gnu")

INSTALL_PY = '''#!/usr/bin/env python3
"""Install the board and bundled native GCC/GDB without downloading anything."""
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / "tools"))
import install_board

def main():
    manifest = json.loads((ROOT / "offline-manifest.json").read_text(encoding="utf-8"))
    flavour, host = install_board.host_flavour()
    systems = {entry["host"]: entry for entry in manifest["systems"]}
    if host not in systems:
        raise RuntimeError("This offline bundle does not contain tools for " + host)
    entry = systems[host]
    arguments = ["--offline", "--version", entry.get("toolchainVersion", manifest["toolchain_version"]),
                 "--archive", str(ROOT / entry["archive"])]
    if entry.get("pythonRuntime"):
        arguments.extend(["--python-runtime", str(ROOT / entry["pythonRuntime"])])
    return install_board.main(arguments + sys.argv[1:])

if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, KeyError) as exc:
        print("offline_install:", exc, file=sys.stderr)
        raise SystemExit(1)
'''

README = '''SoCLab Arduino complete offline bundle

1. Extract this entire ZIP; keep toolchains, hardware and runtimes together.
2. Linux/macOS: run `sh install.sh`. Windows: double-click install.cmd.
   To use a different Arduino sketchbook directory:
     sh install.sh --sketchbook "$HOME/Arduino"
     install.cmd --sketchbook "D:\\document\\Arduino"
3. Restart Arduino IDE after installation.

The bundled archives contain GCC, G++, binutils, GDB, target headers/libraries,
and their upstream notices. No sudo, system PATH edit or network is needed.
Personal boards.local.txt and unrelated platform.local.txt settings are kept.
SocAudio and its examples are included in the board package.

This installer does not configure the FPGA or start an OpenOCD server.
For a Flatpak/Snap IDE use the sketchbook directory visible inside that IDE;
the package cannot change sandbox filesystem or serial-device permissions.

The supplied complete Python runtime includes pyserial. install.sh/install.cmd
use that interpreter, and the installer keeps a copy for later IDE compile and
upload commands. No system Python or pip install is needed with that runtime.
If a package is deliberately built without a runtime, it instead needs an
existing python3/python and pyserial. SSH still requires an SSH client and the
configured board-side services. See docs/INSTALL.md and docs/UPLOAD.md.
'''


def source_files(directory):
    for path in sorted(directory.rglob("*")):
        relative = path.relative_to(directory)
        if (path.is_file()
                and not any(p in (".git", "__pycache__", "build", "dist") for p in relative.parts)
                and not path.name.endswith((".local.txt", ".pyc", ".tmp"))):
            yield path, relative.as_posix()


def runtime_files(directory):
    # A complete Python distribution legitimately contains module directories
    # named build (for example pip/_internal/operations/build). Source-tree
    # exclusions must never strip those runtime modules.
    for path in sorted(directory.rglob("*")):
        if path.is_file():
            yield path, path.relative_to(directory).as_posix()


def relative_runtime_path(value):
    value = value.replace("\\", "/")
    path = PurePosixPath(value)
    if path.is_absolute() or ".." in path.parts or ":" in value or not path.parts:
        raise RuntimeError("Python runtime paths must stay inside runtime-dir")
    return path.as_posix()


def write_text(package, path, text, executable=False):
    info = zipfile.ZipInfo(path)
    info.create_system = 3
    info.external_attr = (stat.S_IFREG | (0o755 if executable else 0o644)) << 16
    info.compress_type = zipfile.ZIP_DEFLATED
    package.writestr(info, text)


def launchers(runtime_paths):
    cases = "\n".join('  %s) python="$root/runtimes/%s" ;;' % (host, path)
                      for host, path in sorted(runtime_paths.items()))
    shell = '''#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
case "$(uname -s):$(uname -m)" in
  Linux:x86_64|Linux:amd64) host=x86_64-linux-gnu ;;
  Linux:aarch64|Linux:arm64) host=aarch64-linux-gnu ;;
  Linux:riscv64) host=riscv64-linux-gnu ;;
  Darwin:x86_64) host=x86_64-apple-darwin ;;
  Darwin:arm64) host=arm64-apple-darwin ;;
  *) printf '%%s\\n' "Unsupported offline host: $(uname -s)/$(uname -m)" >&2; exit 1 ;;
esac
python=python3
case "$host" in
%s
esac
if [ "$python" != python3 ] && [ ! -x "$python" ]; then chmod +x "$python"; fi
exec "$python" "$root/install.py" "$@"
''' % cases
    win_python = runtime_paths.get("x86_64-mingw32")
    if win_python:
        windows = '@echo off\r\n"%%~dp0runtimes\\%s" "%%~dp0install.py" %%*\r\nexit /b %%errorlevel%%\r\n' % win_python.replace("/", "\\")
    else:
        windows = '@echo off\r\npython "%~dp0install.py" %*\r\nexit /b %errorlevel%\r\n'
    return shell, windows


def build_bundle(root, cache, output, hosts, runtime_dir=None):
    platform = root / "hardware/soclab/neorv32"
    metadata = json.loads((platform / "tools/toolchain.json").read_text(encoding="utf-8"))
    version = re.search(r"^version=(.+)$", (platform / "platform.txt").read_text(encoding="utf-8"), re.M).group(1).strip()
    available = {entry["host"]: entry for entry in metadata["systems"]}
    selected = []
    for host in dict.fromkeys(hosts):
        if host not in available:
            raise RuntimeError("No official toolchain asset configured for " + host)
        entry = available[host]
        archive = cache / entry["archiveFileName"]
        if not archive.is_file():
            raise RuntimeError("Cache is missing " + str(archive) + "; download the official archive before packaging")
        check_archive(archive, entry)
        selected.append((host, archive))
    if not selected:
        raise RuntimeError("Choose at least one host")
    runtime_paths = {}
    if runtime_dir is not None:
        runtimes = json.loads((runtime_dir / "manifest.json").read_text(encoding="utf-8"))
        for entry in runtimes["systems"]:
            host, path = entry["host"], relative_runtime_path(entry["python"])
            if host in hosts:
                if not (runtime_dir / path).is_file():
                    raise RuntimeError("Runtime interpreter is missing: " + path)
                runtime_paths[host] = path
        missing = set(hosts) - runtime_paths.keys()
        if missing:
            raise RuntimeError("Runtime manifest has no interpreter for: " + ", ".join(sorted(missing)))
    output.mkdir(parents=True, exist_ok=True)
    # Use a readable host suffix, so single-host and universal assets coexist.
    suffix = "multi-host" if len(selected) > 1 else selected[0][0]
    target = output / ("SoCLab-Arduino-%s-offline-%s.zip" % (version, suffix))
    prefix = "SoCLab-Arduino-%s-offline/" % version
    systems = []
    runtime_roots = set()
    for host, archive in selected:
        system = {"host": host, "archive": "toolchains/" + archive.name,
                  "toolchainVersion": available[host].get("version", metadata["version"])}
        if host in runtime_paths:
            python = PurePosixPath(runtime_paths[host])
            runtime_root = python.parent.parent if python.parent.name == "bin" else python.parent
            runtime_roots.add(runtime_root)
            system.update({"python": "runtimes/" + python.as_posix(),
                           "pythonRuntime": (PurePosixPath("runtimes") / runtime_root).as_posix()})
        systems.append(system)
    manifest = {"version": version, "toolchain_version": metadata["version"], "systems": systems}
    pending = target.with_suffix(".zip.tmp")
    try:
        with zipfile.ZipFile(pending, "w", zipfile.ZIP_DEFLATED, compresslevel=6, allowZip64=True) as package:
            for directory in ("hardware/soclab/neorv32", "libraries/SocAudio", "docs", "LICENSES", "tools"):
                for source, relative in source_files(root / directory):
                    package.write(source, prefix + directory + "/" + relative)
            for name in ("README.md",):
                package.write(root / name, prefix + name)
            if runtime_dir is not None:
                for source, relative in runtime_files(runtime_dir):
                    path = PurePosixPath(relative)
                    if any(root == PurePosixPath(".") or root == path or root in path.parents
                           for root in runtime_roots):
                        package.write(source, prefix + "runtimes/" + relative)
            for host, archive in selected:
                package.write(archive, prefix + "toolchains/" + archive.name, compress_type=zipfile.ZIP_STORED)
            write_text(package, prefix + "offline-manifest.json", json.dumps(manifest, indent=2) + "\n")
            write_text(package, prefix + "install.py", INSTALL_PY)
            shell, windows = launchers(runtime_paths)
            write_text(package, prefix + "install.sh", shell, executable=True)
            write_text(package, prefix + "install.cmd", windows)
            write_text(package, prefix + "OFFLINE-README.txt", README)
        with zipfile.ZipFile(pending) as package:
            corrupt = package.testzip()
            if corrupt:
                raise RuntimeError("Unreadable ZIP entry: " + corrupt)
        pending.replace(target)
    except BaseException:
        pending.unlink(missing_ok=True)
        raise
    return target


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", action="append", help="host triplet; repeat to combine hosts (default Win x64, Linux x64/ARM64)")
    parser.add_argument("--cache", type=Path, required=True, help="directory holding original official toolchain archives")
    parser.add_argument("--output", type=Path, default=ROOT / "dist")
    parser.add_argument("--runtime-dir", type=Path, help="relocatable Python distributions and a manifest.json")
    args = parser.parse_args(argv)
    target = build_bundle(ROOT, args.cache, args.output, args.host or DEFAULT_HOSTS, args.runtime_dir)
    print(target, target.stat().st_size)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, KeyError) as exc:
        print("package_offline:", exc, file=sys.stderr)
        raise SystemExit(1)
