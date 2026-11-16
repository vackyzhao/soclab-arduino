# 安装、更新与离线使用

## 开发板管理器

附加索引地址：

```text
https://raw.githubusercontent.com/vackyzhao/soclab-arduino/main/package_soclab_index.json
```

IDE 会选择当前主机对应的工具包并配置路径。板级包已带 `SocAudio`，无需重复安装库。

Arduino CLI：

```sh
arduino-cli core update-index --additional-urls https://raw.githubusercontent.com/vackyzhao/soclab-arduino/main/package_soclab_index.json
arduino-cli core install soclab:neorv32 --additional-urls https://raw.githubusercontent.com/vackyzhao/soclab-arduino/main/package_soclab_index.json
arduino-cli board listall soclab
```

## 手动安装与上游最新工具链

克隆本仓库后执行 `python3 tools/install_board.py`；Windows 用 `python`。默认读取 IDE 2 的 `~/.arduinoIDE/arduino-cli.yaml` 中 `directories.user`，没有配置时使用平台默认 Arduino sketchbook。

```sh
python3 tools/install_board.py --sketchbook "$HOME/Arduino"
```

每次主动运行安装器时才查询 GitHub 最新稳定版。已有版本可以复用；正常 Arduino 编译不需要网络、不修改系统 PATH。

指定某一官方版本：

```sh
python3 tools/install_board.py --version 15.2.0-1
```

已有工具或下载缓存时离线安装：

```sh
python3 tools/install_board.py --offline --version 15.2.0-1
```

另一台电脑可从官方 [xPack RISC-V GCC Releases](https://github.com/xpack-dev-tools/riscv-none-elf-gcc-xpack/releases) 下载目标系统的原始压缩包，复制过来后：

```sh
python3 tools/install_board.py --offline --version 15.2.0-1 --archive /path/to/xpack-riscv-none-elf-gcc-15.2.0-1-linux-x64.tar.gz
```

不要先手动解压或改压缩包文件名。Windows 使用 `win32-x64.zip`，Linux ARM64 使用 `linux-arm64.tar.gz`，macOS 分别为 `darwin-x64.tar.gz` / `darwin-arm64.tar.gz`。

## 安装目录与故障处理

`platform.local.txt` 是本机配置，里面包含本机工具绝对路径；不要提交 GitHub，也不要直接复制到另一台不同目录的电脑。换电脑应运行安装器重新生成。

- IDE 仍显示旧板卡：关闭并重开 IDE，确认 sketchbook 与安装时一致。
- 找不到编译器：重新运行安装器，或检查 `platform.local.txt` 中路径是否存在。
- Linux 工具无法启动：检查 CPU 架构、glibc 版本和实际错误；当前官方资产不再支持 32 bit Linux/ARM32。
- Windows 报 `bits/c++config.h: No such file or directory` 而文件实际存在：已复现 xPack GCC 的长路径限制。GCC 内部带 `../` 的搜索路径可能超过 Windows 长度限制；本次失败路径为 276 字符。把 Arduino data／sketchbook 设为较短目录，例如 `D:/Arduino`，再安装，通常无需修改源码或手写 include 路径。本次 `D:/document/Arduino` 手动安装和较短 data 目录的 Boards Manager 安装均编译通过。
- GitHub 网络不可用：用 `--offline` 加已有缓存或 `--archive`；编译阶段不需要重新联网。
- 手动 core 与 Boards Manager 同名：手动 `hardware/soclab/neorv32` 优先。切换安装方式前，先把旧手动 core 移到 sketchbook 外保存，避免误以为更新未生效。

本仓库没有向 Arduino 官方 Library Manager 提交收录申请；库目前通过 release ZIP 或随 core 安装。
