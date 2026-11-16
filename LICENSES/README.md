# 许可与来源说明

## 原创适配层

Arduino core、SocAudio 和相关工具由 Chuqi Zhao / SoC Lab group 3 在原工程中维护。原始文件没有单独的 LICENSE；此次整理不擅自把它们改为 MIT 或 BSD 授权。公开 GitHub 源码本身不代表已授予任意使用或再许可权利。

## NEORV32

`hardware/soclab/neorv32/system/neorv32/` 来源于 `kit-kch/psoc-neorv32` 的 NEORV32 v1.11 软件树（源版本 d0157dce）。其根 `LICENSE` 和各源文件的头部声明保持原样。

- 根许可证以及多数驱动为 BSD-3-Clause，copyright Stephan Nolting / NEORV32 contributors。
- `sw/lib/source/neorv32_aux.c` 中的 `neorv32_aux_date2unixtime` 与 `neorv32_aux_unixtime2date` 段落保留 Oryx Embedded SARL 的 GPL-2.0-or-later 声明。请以该文件内实际函数及声明为准；根 BSD 文本不能覆盖这些独立声明。
- 本分发包包含源文件，提供 [GPL v2 完整文本](GPL-2.0-or-later.txt) 供其声明所指的条款查阅。GPL-2.0-or-later 也允许按声明选择后续版本。

## xPack 工具链

GCC/GDB/Newlib 的编译工具不提交在本仓库，也不包含在本仓库生成的板卡压缩包中。安装器和标准索引从 xPack 官方 GitHub 下载各主机资产，保留原压缩包中的许可证、发行信息和源码获取说明。

上游：[xPack RISC-V GCC](https://github.com/xpack-dev-tools/riscv-none-elf-gcc-xpack)、[NEORV32](https://github.com/stnolting/neorv32)、[课程使用的软件分支](https://github.com/kit-kch/psoc-neorv32)。
