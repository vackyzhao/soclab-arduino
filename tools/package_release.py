#!/usr/bin/env python3
"""Build standard Arduino board and library release archives from this repository."""
import argparse
import hashlib
import json
import re
import shutil
import tarfile
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PLATFORM = ROOT / "hardware/soclab/neorv32"
LIBRARY = ROOT / "libraries/SocAudio"


def files(directory):
    for path in sorted(directory.rglob("*")):
        relative = path.relative_to(directory)
        if (path.is_file() and not any(part in ("__pycache__", "build", ".git") for part in relative.parts)
                and not path.name.endswith((".local.txt", ".pyc", ".tmp"))):
            yield path, relative


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "dist")
    parser.add_argument("--base-url", help="release asset directory URL; defaults to this version's GitHub release")
    args = parser.parse_args()
    text = (PLATFORM / "platform.txt").read_text(encoding="utf-8")
    version = re.search(r"^version=(.+)$", text, re.M).group(1).strip()
    library_version = re.search(r"^version=(.+)$", (LIBRARY / "library.properties").read_text(), re.M).group(1).strip()
    base = args.base_url or f"https://github.com/vackyzhao/soclab-arduino/releases/download/v{version}/"
    # A Boards Manager install receives its notices and the same library API.
    shutil.copytree(ROOT / "LICENSES", PLATFORM / "LICENSES", dirs_exist_ok=True)
    shutil.copytree(LIBRARY, PLATFORM / "libraries/SocAudio", dirs_exist_ok=True)
    args.output.mkdir(parents=True, exist_ok=True)
    archive = args.output / f"soclab-neorv32-{version}.tar.bz2"
    with tarfile.open(archive, "w:bz2") as tar:
        for path, relative in files(PLATFORM):
            tar.add(path, arcname=(Path("neorv32") / relative).as_posix(), recursive=False)
    library_zip = args.output / f"SocAudio-{library_version}.zip"
    with zipfile.ZipFile(library_zip, "w", zipfile.ZIP_DEFLATED) as package:
        for path, relative in files(LIBRARY):
            package.write(path, (Path("SocAudio") / relative).as_posix())
    index_path = ROOT / "package_soclab_index.json"
    index = json.loads(index_path.read_text(encoding="utf-8"))
    platform = next(p for p in index["packages"][0]["platforms"] if p["version"] == version)
    platform.update({"url": base.rstrip("/") + "/" + archive.name,
                     "archiveFileName": archive.name,
                     "size": str(archive.stat().st_size),
                     "checksum": "SHA-256:" + hashlib.sha256(archive.read_bytes()).hexdigest()})
    index_path.write_text(json.dumps(index, indent=2) + "\n", encoding="utf-8")
    (args.output / index_path.name).write_bytes(index_path.read_bytes())
    for path in (archive, library_zip, args.output / index_path.name):
        print(path, path.stat().st_size)


if __name__ == "__main__":
    main()
