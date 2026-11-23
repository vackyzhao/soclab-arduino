# 许可与来源说明

## 原创适配层

Arduino core、SocAudio 和相关工具由 Chuqi Zhao / SoC Lab group 3 在原工程中维护。原始文件没有单独的 LICENSE；此次整理不擅自把它们改为 MIT 或 BSD 授权。公开 GitHub 源码本身不代表已授予任意使用或再许可权利。

## NEORV32

`hardware/soclab/neorv32/system/neorv32/` 来源于 [课程使用的软件分支](https://github.com/kit-kch/psoc-neorv32) 的 NEORV32 v1.11 软件树（源版本 d0157dce）；[NEORV32 上游](https://github.com/stnolting/neorv32) 的根 `LICENSE` 和各源文件头部声明保持原样。

- 根许可证以及多数驱动为 BSD-3-Clause，copyright Stephan Nolting / NEORV32 contributors。
- `sw/lib/source/neorv32_aux.c` 中的 `neorv32_aux_date2unixtime` 与 `neorv32_aux_unixtime2date` 段落保留 Oryx Embedded SARL 的 GPL-2.0-or-later 声明。请以该文件内实际函数及声明为准；根 BSD 文本不能覆盖这些独立声明。
- 本分发包包含源文件，提供 [GPL v2 完整文本](GPL-2.0-or-later.txt) 供其声明所指的条款查阅。GPL-2.0-or-later 也允许按声明选择后续版本。

## GCC/GDB 与目标运行库

Windows x64、Linux x64/ARM64 使用 [xPack RISC-V GCC 15.2.0-1](https://github.com/xpack-dev-tools/riscv-none-elf-gcc-xpack/releases/tag/v15.2.0-1) 的官方主机资产。GCC、GDB、binutils、Newlib 及其他组成部分各有自身许可，不能统一标为 BSD；分发时保留上游压缩包中的许可、发行信息、源码地址及适用的运行库例外声明。

RISC-V64 主机包组合 [RISCstar Toolchain](https://riscstar.com/toolchain/) 的 GCC 15.1/GDB 16.3 主机程序与上述 xPack 的 RV32I/ILP32 目标库。该组合由 `tools/package_riscv_host_toolchain.py` 生成，并保留两个来源的完整许可和来源说明：

- [RISCstar 15.1-r1 原始 RV64 主机 / RV32 目标二进制包](https://releases.riscstar.com/toolchain/15.1-r1/riscstar-toolchain-15.1-r1-riscv64-riscv32-none-elf.tar.xz)
- [RISCstar 15.1-r1 对应源码包](https://releases.riscstar.com/toolchain/15.1-r1/riscstar-toolchain-15.1-r1-src.tar.xz)
- [xPack RISC-V GCC 项目与构建来源](https://github.com/xpack-dev-tools/riscv-none-elf-gcc-xpack)

组合包改变了目标库选择，不改变各组成部分的原有许可。RV64 的 QEMU 运行结果也不代表已完成全部 Arduino 示例或原生 RV64 硬件验证，见 [0.3.0 发布说明](../docs/RELEASE_NOTES_0.3.0.md)。

## CPython 与 Python 运行时依赖

0.3.0 的 `soclab-python-runtime` 来自 [python-build-standalone 20261110](https://github.com/astral-sh/python-build-standalone/releases/tag/20261110) 的完整 CPython **3.12.16** `install_only_stripped` 主机资产。包内保留 Python 的 PSF 许可、历史许可及各自带依赖的许可；不能把 OpenSSL、SQLite、压缩库、pip 等依赖一概当作 PSF 授权。

`tools/package_python_runtime.py` 保留完整运行时目录及原有许可文件，并在 `licenses/cpython/` 汇集其许可副本；包根的 `PYTHON-RUNTIME-NOTICES` 说明运行时与 pyserial 的来源。离线 ZIP 和安装至 sketchbook 的运行时均应保留这些声明。

## pyserial

主机串口功能使用 [pyserial 3.5](https://pypi.org/project/pyserial/3.5/)，其源码及完整 BSD 许可见 [pyserial v3.5](https://github.com/pyserial/pyserial/tree/v3.5) 和 [LICENSE.txt](https://raw.githubusercontent.com/pyserial/pyserial/v3.5/LICENSE.txt)。打包工具把纯 Python wheel 安装到运行时的 `site-packages`，并额外保留完整许可于 `licenses/pyserial/LICENSE.txt`；仅保留 wheel 元数据中的短许可字段不足以代替该文本。

## 发布包与 Git 仓库的边界

完整离线 ZIP 包含编译工具的原始二进制压缩包、Python/pyserial 运行时和相应声明；Boards Manager 将 core 与工具作为独立依赖下载。这些大型二进制仅作为 [GitHub Release 资产](https://github.com/vackyzhao/soclab-arduino/releases/tag/v0.3.0) 分发，不提交 Git，也不嵌入单独的 core 或 SocAudio 源码 ZIP。

重新分发时保留所分发组件适用的许可、署名、源码获取说明及其他义务；本说明不替换各组件的原始许可。
