#!/usr/bin/env python3
"""Install/update SoCLab and native GCC/GDB without sudo or PATH edits."""
import argparse
import hashlib
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import tempfile
import urllib.request
from pathlib import Path

API = "https://api.github.com/repos/xpack-dev-tools/riscv-none-elf-gcc-xpack/releases/"
TOOL_NAME = "xpack-riscv-none-elf-gcc"
PYTHON_VERSION = "3.12.16"


def source_platform():
    here = Path(__file__).resolve()
    candidates = (here.parent.parent,
                  here.parent / "hardware/soclab/neorv32",
                  here.parent.parent / "hardware/soclab/neorv32",
                  here.parents[2] / "arduino-board/soclab/neorv32")
    for source in candidates:
        if (source / "boards.txt").is_file():
            return source
    raise RuntimeError("Cannot find the SoCLab platform beside this installer")


def default_sketchbook():
    cfg = Path.home() / ".arduinoIDE/arduino-cli.yaml"
    if cfg.is_file():
        in_dirs = False
        for line in cfg.read_text(encoding="utf-8").splitlines():
            if line.strip() == "directories:":
                in_dirs = True
            elif in_dirs and line and not line[0].isspace():
                in_dirs = False
            elif in_dirs:
                m = re.match(r"\s+user:\s*(.*?)\s*$", line)
                if m and m.group(1):
                    value = m.group(1)
                    if value.startswith('"'):
                        value = json.loads(value)
                    elif value.startswith("'") and value.endswith("'"):
                        value = value[1:-1].replace("''", "'")
                    return Path(value).expanduser()
    return Path.home() / ("Documents/Arduino" if os.name == "nt" else "Arduino")


def host_flavour():
    machine = platform.machine().lower()
    # Windows 11 on ARM64 can run the packaged x64 Python and x64 toolchain.
    # This is the Windows compatibility path, not a native ARM64 compiler.
    if os.name == "nt" and machine in ("amd64", "x86_64", "arm64", "aarch64"):
        return "win32-x64.zip", "x86_64-mingw32"
    if sys.platform.startswith("linux"):
        if machine in ("amd64", "x86_64"):
            return "linux-x64.tar.gz", "x86_64-linux-gnu"
        if machine in ("aarch64", "arm64"):
            return "linux-arm64.tar.gz", "aarch64-linux-gnu"
        if machine == "riscv64":
            return "riscv64-riscv32-none-elf.tar.xz", "riscv64-linux-gnu"
    if sys.platform == "darwin":
        return ("darwin-arm64.tar.gz", "arm64-apple-darwin") if machine == "arm64" else ("darwin-x64.tar.gz", "x86_64-apple-darwin")
    raise RuntimeError("No native official asset for this host: " + sys.platform + "/" + machine)


def fetch(url):
    return urllib.request.urlopen(urllib.request.Request(url, headers={"User-Agent": "SoCLab-Arduino-Installer"}), timeout=60)


def release_tool(version, flavour, host):
    with fetch(API + ("latest" if version is None else "tags/v" + version)) as response:
        release = json.load(response)
    version = release["tag_name"].lstrip("v")
    name = TOOL_NAME + "-" + version + "-" + flavour
    assets = {a["name"]: a for a in release["assets"]}
    if name not in assets:
        raise RuntimeError("Release " + version + " has no " + flavour + " asset")
    asset = assets[name]
    with fetch(assets[name + ".sha"]["browser_download_url"]) as response:
        checksum = response.read().decode("ascii").split()[0]
    return version, {"host": host, "url": asset["browser_download_url"], "archiveFileName": name,
                     "size": str(asset["size"]), "checksum": "SHA-256:" + checksum}


def check_archive(path, asset):
    if asset is None:  # Explicit offline archive, possibly an older release.
        return
    if path.stat().st_size != int(asset["size"]):
        raise RuntimeError("Incomplete official tool archive: " + str(path))
    checksum = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            checksum.update(chunk)
    if "SHA-256:" + checksum.hexdigest() != asset["checksum"]:
        raise RuntimeError("Archive differs from the official package metadata: " + str(path))


def download(asset, cache):
    cache.mkdir(parents=True, exist_ok=True)
    archive = cache / asset["archiveFileName"]
    if archive.is_file():
        check_archive(archive, asset)
        return archive
    partial = archive.with_name(archive.name + ".part")
    print("Downloading", asset["url"], flush=True)
    try:
        with fetch(asset["url"]) as response, partial.open("wb") as f:
            shutil.copyfileobj(response, f, 1024 * 1024)
        check_archive(partial, asset)
        partial.replace(archive)
    except Exception:
        if partial.exists():
            partial.unlink()
        raise
    return archive


def valid_toolchain(directory, prefix="riscv-none-elf-"):
    suffix = ".exe" if os.name == "nt" else ""
    return all((directory / "bin" / (prefix + name + suffix)).is_file() for name in ("gcc", "gdb"))


def unpack(archive, destination, prefix="riscv-none-elf-"):
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="soclab-unpack-", dir=destination.parent) as tmp:
        shutil.unpack_archive(str(archive), tmp)
        roots = [p for p in Path(tmp).iterdir() if p.is_dir() and valid_toolchain(p, prefix)]
        if len(roots) != 1:
            raise RuntimeError("Expected one native GCC/GDB directory in archive (prefix " + prefix + ")")
        if destination.exists():
            raise RuntimeError("Incomplete tool directory exists; move it aside before retrying: " + str(destination))
        roots[0].rename(destination)
    return destination


def check_programs(directory, prefix="riscv-none-elf-"):
    suffix = ".exe" if os.name == "nt" else ""
    for name in ("gcc", "gdb"):
        program = directory / "bin" / (prefix + name + suffix)
        result = subprocess.run([str(program), "--version"], stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                text=True, errors="replace", timeout=20)
        if result.returncode:
            raise RuntimeError(str(program) + " cannot start:\n" + result.stdout)
        print(result.stdout.splitlines()[0], flush=True)


def python_program(directory):
    candidates = (directory / "python.exe",) if os.name == "nt" else (directory / "bin/python3", directory / "bin/python")
    return next((p for p in candidates if p.is_file()), None)


def install_python_runtime(source, sketchbook):
    """Keep the interpreter in the sketchbook after the download folder is removed."""
    source = source.resolve()
    if python_program(source) is None:
        raise RuntimeError("Python runtime has no host interpreter: " + str(source))
    destination = sketchbook / "tools/soclab/python" / PYTHON_VERSION
    if source != destination.resolve():
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copytree(source, destination, dirs_exist_ok=True, symlinks=True)
    program = python_program(destination)
    if program is None:
        raise RuntimeError("Copied Python runtime has no interpreter: " + str(destination))
    result = subprocess.run([str(program), "--version"], stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, errors="replace", timeout=20)
    if result.returncode:
        raise RuntimeError(str(program) + " cannot start:\n" + result.stdout)
    print(result.stdout.strip(), flush=True)
    return program.resolve()


def install_platform(source, sketchbook, toolchain, compiler_prefix=None, python=None):
    target = sketchbook / "hardware/soclab/neorv32"
    if source.resolve() != target.resolve():
        for src in source.rglob("*"):
            if src.is_file() and not src.name.endswith(".local.txt"):
                dst = target / src.relative_to(source)
                dst.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(src, dst)
    local = target / "platform.local.txt"
    text = local.read_text(encoding="utf-8") if local.is_file() else ""
    properties = {"compiler.path": (toolchain.resolve() / "bin").as_posix() + "/"}
    if compiler_prefix is not None:
        properties["compiler.prefix"] = compiler_prefix
    interpreter = python.resolve() if python is not None else Path(sys.executable).resolve()
    properties["soclab.python"] = interpreter.as_posix()
    if os.name == "nt":
        properties["soclab.python.windows"] = interpreter.as_posix()
    lines = [line for line in text.splitlines()
             if not any(re.match(r"\s*" + re.escape(key) + r"\s*=", line) for key in properties)]
    lines.extend(key + "=" + value for key, value in properties.items())
    pending = local.with_name("platform.local.txt.tmp")
    pending.write_text("\n".join(lines) + "\n", encoding="utf-8")
    pending.replace(local)
    return target


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sketchbook", type=Path, default=default_sketchbook())
    parser.add_argument("--version", help="particular release instead of latest stable")
    parser.add_argument("--archive", type=Path, help="official archive copied from another computer")
    parser.add_argument("--offline", action="store_true", help="use installed/cached release without GitHub access")
    parser.add_argument("--python-runtime", type=Path, help="complete Python root to keep inside the Arduino sketchbook")
    args = parser.parse_args(argv)
    source = source_platform()
    flavour, host = host_flavour()
    pinned = json.loads((source / "tools/toolchain.json").read_text(encoding="utf-8"))
    pinned_asset = next((s for s in pinned["systems"] if s["host"] == host), None)
    if host == "riscv64-linux-gnu" and pinned_asset is None:
        raise RuntimeError("No native RISC-V host toolchain is configured in tools/toolchain.json")
    provider = pinned_asset.get("provider", "xpack") if pinned_asset else "xpack"
    prefix = pinned_asset.get("compilerPrefix", "riscv-none-elf-") if pinned_asset else "riscv-none-elf-"
    tool_name = TOOL_NAME if provider == "xpack" else provider + "-toolchain"
    if provider != "xpack":
        # RISCstar publishes a native RV64-host/RV32-target package separately
        # from xPack. Never ask the xPack API for an unsupported host.
        version = args.version.lstrip("v") if args.version else pinned_asset.get("version", pinned["version"])
        asset = pinned_asset if version == pinned_asset.get("version", pinned["version"]) else None
        if asset is None and args.archive is None:
            raise RuntimeError("Provider " + provider + " has no configured archive for " + version + "; supply --archive")
    elif args.offline:
        version = args.version.lstrip("v") if args.version else None
        if args.archive and version is None:
            match = re.fullmatch(re.escape(TOOL_NAME) + r"-(.+)-" + re.escape(flavour), args.archive.name)
            if not match:
                raise RuntimeError("Use an official archive filename or specify --version")
            version = match.group(1)
        version = version or pinned["version"]
        asset = pinned_asset if version == pinned["version"] else None
    else:
        version, asset = release_tool(args.version.lstrip("v") if args.version else None, flavour, host)
    destination = args.sketchbook / "tools/soclab" / tool_name / version
    if valid_toolchain(destination, prefix):
        directory = destination
        print("Reusing installed", directory, flush=True)
    else:
        archive = args.archive
        if archive is None:
            if asset is None:
                raise RuntimeError("Offline older release requires --archive")
            cache = args.sketchbook / "tools/soclab/downloads"
            archive = cache / asset["archiveFileName"] if args.offline else download(asset, cache)
        check_archive(archive, asset)
        directory = unpack(archive, destination, prefix)
    check_programs(directory, prefix)
    python = install_python_runtime(args.python_runtime, args.sketchbook) if args.python_runtime else None
    target = install_platform(source, args.sketchbook, directory, prefix, python)
    print("Installed board:", target)
    print("Configured:", target / "platform.local.txt")
    print("No sudo or PATH edit needed; future compilation uses the installed files offline.")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, KeyError, subprocess.SubprocessError) as exc:
        print("install_board:", exc, file=sys.stderr)
        raise SystemExit(1)
