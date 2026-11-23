#!/usr/bin/env python3
"""Package a complete python-build-standalone runtime and pyserial for Arduino.

Inputs are downloaded separately from their official distributors. This script
does not download or publish anything and never runs the supplied interpreter.
Both install_only archives (python/...) and full archives (python/install/...)
are supported. stdout is a JSON object suitable for an Arduino tool system.
"""
import argparse
import copy
import hashlib
import io
import json
import posixpath
import re
import tarfile
import zipfile
from pathlib import Path, PurePosixPath


ROOT = "python-runtime"


def archive_path(name):
    """Return a relative POSIX path without traversal or platform drive paths."""
    if "\\" in name or name.startswith("/") or re.match(r"^[A-Za-z]:", name):
        raise ValueError("Archive contains a non-relative path")
    path = PurePosixPath(name)
    if ".." in path.parts:
        raise ValueError("Archive contains a parent path")
    return path.as_posix().rstrip("/")


def is_notice(name):
    return any(re.match(r"^(licenses?|copying|notices?)([._-].*)?$", part, re.I)
               for part in PurePosixPath(name).parts)


class RuntimeArchive:
    """Write parents before children, including symlink-only directories.

    Arduino's extractor creates a symlink placeholder before resolving links.
    Unlike Python's tar extractor, it does not create that placeholder's parent
    directory. PBS archives can omit directory entries, so supply them here.
    """
    def __init__(self, archive):
        self.archive = archive
        self.directories = set()

    def addfile(self, entry, content=None):
        name = entry.name.rstrip("/")
        parents = list(PurePosixPath(name).parents)
        for parent in reversed(parents):
            parent = parent.as_posix()
            if parent == "." or parent in self.directories:
                continue
            directory = tarfile.TarInfo(parent)
            directory.type = tarfile.DIRTYPE
            directory.mode = 0o755
            self.archive.addfile(directory)
            self.directories.add(parent)
        if entry.isdir():
            if name in self.directories:
                return
            self.directories.add(name)
        self.archive.addfile(entry, content)


def add_bytes(output, name, data, mode=0o644):
    entry = tarfile.TarInfo(name)
    entry.size = len(data)
    entry.mode = mode
    output.addfile(entry, io.BytesIO(data))


def runtime_layout(members, windows):
    names = {archive_path(member.name) for member in members}
    executable = "python.exe" if windows else "bin/python3"
    for prefix in ("python/", "python/install/"):
        if prefix + executable in names:
            break
    else:
        raise ValueError("No complete CPython interpreter found at python/ or python/install/")
    if windows:
        site = "Lib/site-packages"
        if not any(name.startswith(prefix + "Lib/") for name in names):
            raise ValueError("Windows runtime has no complete standard library")
    else:
        versions = {match.group(1) for name in names
                    for match in [re.match(re.escape(prefix) + r"lib/(python3\.\d+)/", name)]
                    if match}
        if len(versions) != 1:
            raise ValueError("Expected one CPython 3 standard library directory")
        site = "lib/" + versions.pop() + "/site-packages"
    return prefix, executable, site


def build_runtime(python_archive, pyserial_wheel, host, output, pyserial_license=None):
    if not re.fullmatch(r"[A-Za-z0-9_.-]+", host):
        raise ValueError("Invalid Arduino host name")
    windows = "mingw" in host.lower() or "windows" in host.lower()
    output = Path(output)
    if not output.name.endswith((".tar.gz", ".tgz")):
        output = output / ("soclab-python-runtime-" + host + ".tar.gz")
    inputs = (Path(python_archive), Path(pyserial_wheel))
    if output.resolve() in (path.resolve() for path in inputs):
        raise ValueError("Output must differ from the input archives")
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_name(output.name + ".tmp")
    try:
        with tarfile.open(inputs[0], "r:gz") as source, zipfile.ZipFile(inputs[1]) as wheel:
            members = source.getmembers()
            prefix, executable, site = runtime_layout(members, windows)
            wheel_entries = [entry for entry in wheel.infolist() if not entry.is_dir()]
            wheel_names = [archive_path(entry.filename) for entry in wheel_entries]
            wheel_metadata = [name for name in wheel_names if name.endswith(".dist-info/WHEEL")]
            if len(wheel_metadata) != 1:
                raise ValueError("Expected one pyserial wheel metadata directory")
            metadata = wheel.read(wheel_metadata[0]).decode("utf-8")
            if not re.search(r"^Root-Is-Purelib:\s*true\s*$", metadata, re.M | re.I):
                raise ValueError("pyserial wheel must be pure Python")
            tags = re.findall(r"^Tag:\s*(\S+)\s*$", metadata, re.M)
            if not tags or any(not tag.endswith("-none-any") for tag in tags):
                raise ValueError("pyserial wheel must be independent of platform and ABI")
            if "serial/__init__.py" not in wheel_names:
                raise ValueError("Wheel does not contain the serial package")
            cpython_notices = []
            serial_notices = []
            with tarfile.open(temporary, "w:gz") as target_archive:
                target = RuntimeArchive(target_archive)
                for member in members:
                    name = archive_path(member.name)
                    notice_data = None
                    if member.isfile() and is_notice(name):
                        notice_data = source.extractfile(member).read()
                        destination = ROOT + "/licenses/cpython/" + name
                        add_bytes(target, destination, notice_data)
                        cpython_notices.append(destination)
                    if not name.startswith(prefix):
                        continue
                    relative = name[len(prefix):]
                    if not relative:
                        continue
                    entry = copy.copy(member)
                    entry.name = ROOT + "/" + relative
                    if not (entry.isfile() or entry.isdir() or entry.issym() or entry.islnk()):
                        raise ValueError("Runtime archive has an unsupported entry type")
                    if entry.issym():
                        if "\\" in entry.linkname or entry.linkname.startswith("/"):
                            raise ValueError("Runtime symlink has a non-relative target")
                        resolved = posixpath.normpath(posixpath.join(posixpath.dirname(entry.name), entry.linkname))
                        if not resolved.startswith(ROOT + "/"):
                            raise ValueError("Runtime symlink leaves the runtime directory")
                    elif entry.islnk():
                        linked = archive_path(entry.linkname)
                        if not linked.startswith(prefix):
                            raise ValueError("Runtime hard link leaves the installation directory")
                        entry.linkname = ROOT + "/" + linked[len(prefix):]
                    if entry.isfile():
                        # Reuse notice bytes: seeking backwards in the input
                        # gzip stream would decompress its entire prefix again.
                        content = io.BytesIO(notice_data) if notice_data is not None else source.extractfile(member)
                    else:
                        content = None
                    target.addfile(entry, content)
                for entry, name in zip(wheel_entries, wheel_names):
                    if (entry.external_attr >> 16) & 0o170000 == 0o120000:
                        raise ValueError("Wheel must not contain symbolic links")
                    destination = name
                    if ".data/" in name:
                        section = name.split(".data/", 1)[1]
                        if not section.startswith("purelib/"):
                            raise ValueError("Unsupported non-purelib wheel data")
                        destination = section[len("purelib/"):]
                    data = wheel.read(entry)
                    add_bytes(target, ROOT + "/" + site + "/" + destination, data)
                    if is_notice(name):
                        notice = ROOT + "/licenses/pyserial/" + name
                        add_bytes(target, notice, data)
                        serial_notices.append(notice)
                if pyserial_license is not None:
                    license_data = Path(pyserial_license).read_bytes()
                    if not license_data.strip():
                        raise ValueError("Supplemental pyserial license is empty")
                    notice = ROOT + "/licenses/pyserial/LICENSE.txt"
                    add_bytes(target, notice, license_data)
                    serial_notices.append(notice)
                if not cpython_notices or not serial_notices:
                    raise ValueError("Both CPython and pyserial license notices are required; "
                                     "older pyserial wheels need --pyserial-license from upstream")
                notices = (
                    "SoCLab Arduino Python runtime\n\n"
                    "This archive redistributes a complete CPython runtime from\n"
                    "python-build-standalone and the pure-Python pyserial package.\n"
                    "All runtime files and original license notices are retained.\n"
                    "For CPython and its bundled dependencies, see licenses/cpython/.\n"
                    "For pyserial, see licenses/pyserial/ and its installed dist-info.\n"
                    "The collected notices are copies; upstream license terms apply.\n\n"
                    "CPython source archive: " + inputs[0].name + "\n"
                    "pyserial source wheel: " + inputs[1].name + "\n"
                    "Arduino host: " + host + "\n"
                )
                if pyserial_license is not None:
                    notices += "Supplemental pyserial license: " + Path(pyserial_license).name + "\n"
                add_bytes(target, ROOT + "/PYTHON-RUNTIME-NOTICES.txt", notices.encode("utf-8"))
        temporary.replace(output)
    except Exception:
        temporary.unlink(missing_ok=True)
        raise
    digest = hashlib.sha256()
    with output.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return {
        "host": host,
        "archiveFileName": output.name,
        "size": str(output.stat().st_size),
        "checksum": "SHA-256:" + digest.hexdigest(),
        "executable": executable,
        "sitePackages": site,
        "output": str(output.resolve()),
        "cpythonNotices": len(cpython_notices),
        "pyserialNotices": len(serial_notices),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--python-archive", required=True, type=Path)
    parser.add_argument("--pyserial-wheel", required=True, type=Path)
    parser.add_argument("--pyserial-license", type=Path,
                        help="official upstream license file, required if an older wheel omits it")
    parser.add_argument("--host", required=True)
    parser.add_argument("--output", required=True, type=Path,
                        help="output .tar.gz path or output directory")
    args = parser.parse_args()
    try:
        result = build_runtime(args.python_archive, args.pyserial_wheel, args.host, args.output,
                               args.pyserial_license)
    except (OSError, ValueError, tarfile.TarError, zipfile.BadZipFile) as exc:
        parser.exit(1, "package_python_runtime: " + str(exc) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
