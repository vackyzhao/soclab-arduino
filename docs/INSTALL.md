# 安装、更新与离线使用

0.3.0 的完整离线包同时带 Arduino core、SocAudio 的 25 个示例、GCC/G++/binutils/GDB、目标头文件/库、CPython 3.12.16 和 pyserial 3.5。安装后编译及本机 UART 上传不依赖系统 Python、pip、PATH 修改或网络；Arduino IDE 本身及 FPGA bitstream 需另行准备。

## 开发板管理器安装或直接更新

在 Arduino IDE 的 **文件 → 首选项 → 附加开发板管理器网址** 添加：

```text
https://raw.githubusercontent.com/vackyzhao/soclab-arduino/main/package_soclab_index.json
```

搜索 **KIT SoC Lab NEORV32**，选择 **0.3.0** 并点击安装或更新。已有 Boards Manager 安装可以直接更新，无需先删除。管理器下载对应主机的 GCC/GDB 和 Python 工具依赖并配置路径，`SocAudio` 已包含在板包中。

Arduino CLI 对应操作：

```sh
arduino-cli core update-index --additional-urls https://raw.githubusercontent.com/vackyzhao/soclab-arduino/main/package_soclab_index.json
arduino-cli core install soclab:neorv32@0.3.0 --additional-urls https://raw.githubusercontent.com/vackyzhao/soclab-arduino/main/package_soclab_index.json
arduino-cli board listall soclab
```

管理器版本目录是 `<Arduino data>/packages/soclab/hardware/neorv32/<版本>`。若在旧版本旁配置过 `boards.local.txt`，升级后检查新版本目录并迁入自己的连接设置；不要假定 IDE 自动迁移该本地文件，也不要复制旧的编译器绝对路径。

## 完整离线包安装或更新手动 core

从 [0.3.0 Release](https://github.com/vackyzhao/soclab-arduino/releases/tag/v0.3.0) 下载与运行 Arduino IDE 的电脑匹配的包。名称格式为 `SoCLab-Arduino-0.3.0-offline-<triplet>.zip`：

| 主机 | triplet | 适配说明 |
| --- | --- | --- |
| Windows x64 | `x86_64-mingw32` | x64 Python 与 GCC/GDB |
| Windows 11 ARM64 | `x86_64-mingw32` | 使用 Windows x64 仿真；非原生 ARM64，尚未实机验证 |
| Linux x64 | `x86_64-linux-gnu` | 需要兼容的 glibc 环境 |
| Linux ARM64 | `aarch64-linux-gnu` | 需要兼容的 glibc 环境，未作 ARM64 实机验证 |
| Linux RISC-V64 | `riscv64-linux-gnu` | glibc ≥ 2.38；RISCstar 15.1/GDB 16.3 主机工具与 xPack RV32I/ILP32 目标库组合 |

此处架构是**电脑主机**的架构；所有包都为 FPGA 内的 RV32 NEORV32 编译程序。RV64 组合工具已在 QEMU 下运行 GCC/GDB，并将 RV32I C/C++ 程序在 `qemu-riscv32` 执行通过；尚未完成 RV64 主机的全套 Arduino 示例或原生硬件验证。不提供 32 bit Linux/ARM32、musl/Alpine 或 macOS 完整离线包的支持声明。

先在 IDE 首选项查看 **项目文件夹位置 / Sketchbook location**，完整解压 ZIP 后进入解压目录。不要只取 `hardware` 子目录，启动器还会使用 `toolchains`、`runtimes` 和 `tools`。

Windows 双击 `install.cmd`，或在 PowerShell 明确指定实际 sketchbook：

```powershell
.\install.cmd --sketchbook "D:\Arduino"
```

Linux：

```sh
sh install.sh --sketchbook "$HOME/Arduino"
```

路径仅为示例，学校电脑、Flatpak/Snap 或自定义 IDE 配置应填自己的实际目录。省略 `--sketchbook` 时，安装器尝试读取 IDE 2 的 `~/.arduinoIDE/arduino-cli.yaml`；找不到时才使用平台默认目录。

启动器直接运行包内的 Python，不需要先安装系统 Python。安装器把 core 和主机工具放入：

```text
<sketchbook>/hardware/soclab/neorv32/
<sketchbook>/tools/soclab/<工具提供方>/<版本>/
<sketchbook>/tools/soclab/python/3.12.16/
```

它在 `platform.local.txt` 写入编译器和 Python 的绝对路径，保留 `boards.local.txt` 及其他无关的本地属性。以后即使移除下载解压目录，已安装的 Python 仍留在 sketchbook。更新已有手动 core 时运行同一安装器即可；完成后完全关闭并重新打开 IDE。

完整包的安装不需要 sudo、不改系统 PATH、不联网，也不改操作系统串口权限。Flatpak/Snap IDE 必须能访问实际 sketchbook 和所选串口；安装器不改变这些应用权限。UART-over-SSH 仍需要操作系统的 OpenSSH `ssh`，板端 Python/服务和非交互 SSH 配置也需事先可用，见 [上传说明](UPLOAD.md)。

## 确认更新的是正在使用的副本

`<sketchbook>/hardware/soclab/neorv32` 的手动 core 优先于 Boards Manager 的同名 core。已有手动安装时，使用完整离线安装器更新该目录。若主动迁移到 Boards Manager，先在 sketchbook 外保存旧手动目录和本地连接设置，再按管理器方式安装；不要直接删除唯一的本地配置。

下载源码 ZIP、`git pull` 或安装 `SocAudio-0.3.0.zip` 只更新下载目录或库，**不会自动更新已安装的 core、Upload method 菜单和上传 recipe**。库 ZIP 也不安装 GCC/GDB/Python。需要完整功能时应更新板包。

IDE 的详细编译/上传输出应显示实际使用的平台目录和工具绝对路径。0.3.0 的 KR260 菜单包含 `gdb`、`uartssh`、`uartlocal`，SocAudio 示例包含 `AnalogWriteStereo`、`DacResolution` 和 `BreathingLED`。菜单出现只证明软件已安装，运行仍需匹配 bitstream。

## 从源码安装及选择工具链

希望直接使用源码或主动跟进上游工具链时，可用已有 Python 执行：

```sh
git clone https://github.com/vackyzhao/soclab-arduino.git
cd soclab-arduino
python3 tools/install_board.py --sketchbook "$HOME/Arduino"
```

Windows 将 `python3` 换成当前可用 Python 的完整路径或 `python`。这种源码安装方式需要现有 Python；它不等同于带 Python 的完整离线包。没有传入 `--python-runtime` 时，安装器把当前解释器的绝对路径记录到 `platform.local.txt`。本机串口上传还需要在该解释器中安装 pyserial。

Windows x64、Linux x64/ARM64 的源码安装器默认查询 xPack 官方最新稳定版；指定版本或用已有官方原始压缩包离线安装：

```sh
python3 tools/install_board.py --version 15.2.0-1 --sketchbook "$HOME/Arduino"
python3 tools/install_board.py --offline --version 15.2.0-1 --archive /path/to/xpack-riscv-none-elf-gcc-15.2.0-1-linux-x64.tar.gz --sketchbook "$HOME/Arduino"
```

不要预先解压或改名工具链原始资产；主机必须与压缩包对应。RISC-V64 使用仓库中明确配置的 SoCLab 组合工具包，不向 xPack 请求不存在的 RV64 主机资产。macOS 的源码工具链映射仍保留，但本次未提供带 Python 的 macOS 完整离线包。

## 常见问题

- **IDE 仍显示旧菜单或示例：** 完全重启 IDE，核对实际 sketchbook 和编译输出的平台目录；检查是否有优先使用的手动 core。
- **找不到 Python 或 pyserial：** 完整包用户重新运行该包的安装器，检查 `platform.local.txt` 中的 `soclab.python`；不要靠修改系统 PATH 解决另一份旧 core。源码自选解释器用户须向同一个解释器安装依赖。
- **找不到编译器：** 检查本地配置中的工具绝对路径是否仍存在；换电脑或移动 sketchbook 后重新运行安装器。
- **Linux 程序无法启动：** 检查主机架构、glibc 和实际报错。RV64 需要 glibc ≥ 2.38；不能用 ARM64 包替代 RV64 包，也不把 musl 环境视为兼容的 glibc。
- **Windows 的 `bits/c++config.h` 实际存在却报找不到：** xPack GCC 曾复现长路径问题，可将 sketchbook/Arduino data 放到较短目录，例如 `D:/Arduino`，再运行安装器；不要先改源码 include 路径。
- **能传输但程序不运行：** 更新正在使用的 core；KR260 GDB 需从 `0xE0000000` resume，UART 需完成 loader 的 `e` 命令，见 [上传说明](UPLOAD.md)。

本仓库尚未向 Arduino 官方 Library Manager 提交收录申请；SocAudio 通过 Release ZIP 或随 core 安装。工具和运行时的来源及许可见 [LICENSES/README.md](../LICENSES/README.md)。
