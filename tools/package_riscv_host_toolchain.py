"""Compose a native Linux/RV64GC NEORV32 toolchain from upstream archives.

RISCstar's GCC/GDB host programs stay unchanged. Its RV32IMAC runtime is
replaced by xPack's host-independent RV32I/ILP32 target libraries and headers.
The compiler accurately reports GCC 15.1.0; the runtime comes from GCC 15.2.0.
This composition is restricted to NEORV32 targets and must be compile tested.

Sources:
https://riscstar.com/toolchain/
https://releases.riscstar.com/toolchain/15.1-r1/riscstar-toolchain-15.1-r1-riscv64-riscv32-none-elf.tar.xz
https://releases.riscstar.com/toolchain/15.1-r1/riscstar-toolchain-15.1-r1-src.tar.xz
https://github.com/xpack-dev-tools/riscv-none-elf-gcc-xpack/releases/tag/v15.2.0-1

Use --riscstar-archive and --output for a new reproducible package, or
--native-root to apply the runtime overlay to a disposable extracted copy.
"""
import argparse
import json
from pathlib import Path
import shutil
import tarfile


def main():
    p = argparse.ArgumentParser(description=__doc__)
    source = p.add_mutually_exclusive_group(required=True)
    source.add_argument('--native-root', type=Path)
    source.add_argument('--riscstar-archive', type=Path)
    p.add_argument('--xpack-archive', type=Path, required=True)
    p.add_argument('--runtime-arch', choices=['rv32i', 'rv32imc'], default='rv32i')
    p.add_argument('--work-dir', type=Path)
    p.add_argument('--output', type=Path)
    args = p.parse_args()
    if args.riscstar_archive:
        if not args.work_dir or not args.output:
            p.error('--riscstar-archive requires --work-dir and --output')
        work = args.work_dir.resolve()
        work.mkdir(parents=True, exist_ok=False)
        with tarfile.open(args.riscstar_archive, 'r:*') as archive:
            archive.extractall(work, filter='data')
        original = work / 'riscstar-toolchain-15.1-r1-riscv64-riscv32-none-elf'
        root = work / 'soclab-riscv-toolchain-2026.11.23'
        original.rename(root)
    else:
        root = args.native_root.resolve()
    if not (root / 'bin/riscv32-none-elf-gcc').is_file():
        raise SystemExit('Not an extracted RISCstar toolchain')
    target = root / 'riscv32-none-elf'
    gccdir = root / 'lib/gcc/riscv32-none-elf/15.1.0'
    if not gccdir.is_dir():
        raise SystemExit('Expected GCC 15.1.0 directory not found')
    # Only this explicitly supplied disposable copy is modified. Remove all old
    # target runtime libraries so there cannot be an RV32IMAC fallback.
    for old in [target / 'lib', target / 'include']:
        if old.exists():
            shutil.rmtree(old)
    for old in list(gccdir.rglob('*')):
        if old.is_file() and old.suffix in {'.a', '.o', '.specs'}:
            old.unlink()
    counts = {'target_libraries': 0, 'gcc_runtime': 0, 'headers': 0,
              'cxx_target_headers': 0, 'license_files': 0}
    prefix = 'xpack-riscv-none-elf-gcc-15.2.0-1/'
    archlib = f'riscv-none-elf/lib/{args.runtime_arch}/ilp32/'
    gccruntime = f'lib/gcc/riscv-none-elf/15.2.0/{args.runtime_arch}/ilp32/'
    cppbase = 'riscv-none-elf/include/c++/15.2.0/'
    cpparch = f'{cppbase}riscv-none-elf/{args.runtime_arch}/ilp32/'
    with tarfile.open(args.xpack_archive, 'r:*') as archive:
        for member in archive:
            if not member.isfile() or not member.name.startswith(prefix):
                continue
            rel = member.name[len(prefix):]
            dest = None
            category = None
            if rel.startswith(archlib):
                dest = target / 'lib' / rel[len(archlib):]
                category = 'target_libraries'
            elif rel.startswith(gccruntime):
                dest = gccdir / rel[len(gccruntime):]
                category = 'gcc_runtime'
            elif rel.startswith(cpparch):
                dest = target / 'include/c++/15.1.0/riscv32-none-elf' / rel[len(cpparch):]
                category = 'cxx_target_headers'
            elif rel.startswith(cppbase):
                tail = rel[len(cppbase):]
                if tail.startswith('riscv-none-elf/'):
                    continue
                dest = target / 'include/c++/15.1.0' / tail
                category = 'headers'
            elif rel.startswith('riscv-none-elf/include/'):
                dest = target / 'include' / rel[len('riscv-none-elf/include/'):]
                category = 'headers'
            elif rel.startswith('distro-info/'):
                dest = root / 'share/xpack-target-runtime' / rel
                category = 'license_files'
            elif rel.startswith(('share/licenses/', 'share/doc/', 'licenses/')) and any(
                    name in rel.upper() for name in ['COPYING', 'LICENSE']):
                dest = root / 'share/xpack-target-runtime' / rel
                category = 'license_files'
            if dest is None:
                continue
            if not dest.resolve().is_relative_to(root):
                raise SystemExit(f'Unexpected archive path: {member.name}')
            dest.parent.mkdir(parents=True, exist_ok=True)
            with archive.extractfile(member) as src, dest.open('wb') as dst:
                shutil.copyfileobj(src, dst)
            counts[category] += 1
    required = [target / 'lib/libc.a', target / 'lib/libm.a',
                target / 'lib/libstdc++.a', target / 'lib/nosys.specs',
                target / 'lib/nano.specs', gccdir / 'libgcc.a',
                target / 'include/stdio.h',
                target / 'include/c++/15.1.0/riscv32-none-elf/bits/c++config.h']
    missing = [str(path) for path in required if not path.is_file()]
    if missing:
        raise SystemExit('Missing expected runtime components: ' + repr(missing))
    for program in (root / 'bin').glob('riscv32-none-elf-*'):
        suffix = program.name[len('riscv32-none-elf-'):]
        wrapper = root / 'bin' / ('riscv-none-elf-' + suffix)
        wrapper.write_text('#!/bin/sh\nselfdir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\n'
                           f'exec "$selfdir/riscv32-none-elf-{suffix}" "$@"\n',
                           encoding='utf-8')
        wrapper.chmod(0o755)
    report = {'status': 'prepared_for_compile_and_link_validation',
              'host_compiler': 'RISCstar 15.1-r1 GCC 15.1.0 native RV64GC Linux',
              'target_runtime': 'xPack 15.2.0-1 target-only libraries and headers',
              'target_runtime_arch': args.runtime_arch, 'abi': 'ilp32',
              'counts': counts,
              'scope': 'NEORV32-only; native compiler multilib metadata unchanged',
              'sources': [
                  'https://releases.riscstar.com/toolchain/15.1-r1/riscstar-toolchain-15.1-r1-riscv64-riscv32-none-elf.tar.xz',
                  'https://releases.riscstar.com/toolchain/15.1-r1/riscstar-toolchain-15.1-r1-src.tar.xz',
                  'https://github.com/xpack-dev-tools/riscv-none-elf-gcc-xpack/releases/tag/v15.2.0-1'],
              'host_requirements': 'GNU/Linux riscv64 RV64GC; glibc >= 2.38',
              'compiler_prefix': 'riscv-none-elf- (shell wrappers to native upstream names)'}
    (root / 'SOCLAB_TARGET_RUNTIME.json').write_text(
        json.dumps(report, indent=2) + '\n', encoding='utf-8')
    (root / 'SOCLAB_TOOLCHAIN_README.txt').write_text(
        'SoCLab Linux RISC-V64 host toolchain\n\n'
        'Host: RISCstar 15.1-r1, native RV64GC Linux, GCC 15.1.0 and GDB.\n'
        'Target runtime: xPack 15.2.0-1 RV32I/ILP32 newlib, libm, libgcc and libstdc++.\n'
        'Target scope: NEORV32 RV32IMC_Zicsr_Zifencei/ILP32. Always pass board -march/-mabi flags.\n'
        'Upstream compiler multilib listings describe RISCstar defaults, not this NEORV32-only runtime.\n'
        'All upstream host programs remain unchanged. riscv-none-elf-* shell wrappers provide the Arduino prefix.\n'
        'The replacement target libraries contain no atomic A or floating F/D requirement.\n'
        'Licenses and complete upstream source links are provided in share/ and SOCLAB_TARGET_RUNTIME.json.\n'
        'Reproduce with tools/package_riscv_host_toolchain.py from the SoCLab Arduino repository.\n',
        encoding='utf-8')
    if args.output:
        output = args.output.resolve()
        output.parent.mkdir(parents=True, exist_ok=True)
        with tarfile.open(output, 'w:gz', compresslevel=6) as archive:
            archive.add(root, arcname=root.name)
        report['output'] = str(output)
        report['output_bytes'] = output.stat().st_size
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
