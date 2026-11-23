# 0.3.0 发布说明

0.3.0 将扩展后的 SocAudio/Arduino API、三种 KR260 上传方式以及 Windows/Linux 完整离线安装整合为同一版板包。Release 页面：[v0.3.0](https://github.com/vackyzhao/soclab-arduino/releases/tag/v0.3.0)。

## API 与 25 个示例

- 新增 `DAC_LEFT` / `DAC_RIGHT` 虚拟引脚，通过现有 FIFO 和 FPGA ΣΔ DAC 实现 `analogWrite()` 平均电平输出。`analogWriteResolution(1..24)` 控制输入范围，默认 8 bit；需包含 `<SocAudio.h>`。普通 GPIO 仍按阈值输出 HIGH/LOW。
- `analogWrite`、显式 PCM 输出和 `Synth` 之间切换时转交音频外设，避免两个生成路径同时向 FIFO 送数据。DAC 输出需合适的模拟滤波；交流耦合音频电路不能直接测量直流电平。
- 示例从 10 个扩展到 **25 个**。新增 15 个：`AnalogWriteStereo`、`BlinkWithoutDelay`、`BreathingLED`、`CourseBlinky`、`CourseDacSine`、`CourseDacSineIRQ`、`DacResolution`、`NonBlockingMelody`、`PcmClipPlayer`、`PitchGlide`、`SerialCommandConsole`、`StereoChannelTest`、`SynthSerialControl`、`TimingAndRandom`、`WaveformPlayer`。

示例的准备条件、执行方式和预期结果见 [中文示例导览](../libraries/SocAudio/EXAMPLES.md)，DAC 行为见 [analogWrite 说明](../libraries/SocAudio/ANALOG_WRITE.md)。

## 上传后自动执行

KR260 的 **Upload method** 包含 `gdb`、`uartssh` 和 `uartlocal`。本机/远端 OpenOCD 地址可以在 `boards.local.txt` 自定义；UART SSH 目标独立配置，可使用局域网/Tailscale IP 或 SSH 主机别名。

KR260 GDB 现在执行 `monitor reset halt → load → monitor resume 0xE0000000 → detach`，避免在带 loader 的硬件上仅加载后又回到 boot ROM 等待。ZedBoard 仍使用 `load → monitor reset run → detach` 完成其 XIP 初始化。

UART 以 115200、8N1 发送原生 `u` 上传命令，核对 loader 的 `OK` 后发送 `e` 并确认启动，不需要额外手动 GDB `run`。UART-over-SSH 需要匹配的 `lab3_kr260_uart` FPGA app/设备树、PS 串口和 `0xA0030000` 独立 reset GPIO；本机串口路径需要 loader 已经运行，不能假定 DTR/RTS 可以复位 CPU。详细配置见 [上传说明](UPLOAD.md)。

## 随板包安装主机工具

完整离线包包含 GCC/G++/binutils/GDB、RV32 目标头文件/库、CPython **3.12.16** 和 pyserial **3.5**。Boards Manager 在对应主机上下载匹配的工具依赖；完整离线 ZIP 直接带有它们，不依赖系统 Python、pip、sudo、PATH 修改或安装时联网。

| 完整离线资产 | 主机适配 |
| --- | --- |
| `SoCLab-Arduino-0.3.0-offline-x86_64-mingw32.zip` | Windows x64；Windows 11 ARM64 经 x64 仿真使用同一包 |
| `SoCLab-Arduino-0.3.0-offline-x86_64-linux-gnu.zip` | Linux x64 / glibc |
| `SoCLab-Arduino-0.3.0-offline-aarch64-linux-gnu.zip` | Linux ARM64 / glibc |
| `SoCLab-Arduino-0.3.0-offline-riscv64-linux-gnu.zip` | Linux RISC-V64 / glibc ≥ 2.38 |

Windows x64、Linux x64/ARM64 的编译工具为 xPack **15.2.0-1**。RV64 主机使用 SoCLab 组合包：RISCstar **GCC 15.1 / GDB 16.3** 主机工具，加上 xPack **15.2.0-1** 的 RV32I/ILP32 目标库；主机工具使用 RV64GC，不代表 FPGA 的 RV32 程序需要 A/F/D 扩展。源码与许可链接见 [第三方声明](../LICENSES/README.md)。

## 如何更新

**Boards Manager 安装：** 搜索 KIT SoC Lab NEORV32，直接更新到 0.3.0，无需先删除旧版本。若使用自定义 `boards.local.txt`，检查新版本目录是否已有自己的地址配置。

**手动安装：** 完整解压对应离线 ZIP。Windows 双击 `install.cmd`，或执行 `install.cmd --sketchbook "D:\Arduino"`；Linux 执行 `sh install.sh --sketchbook "$HOME/Arduino"`。路径须使用 IDE 首选项显示的实际 sketchbook。安装器保留本地连接配置，重新写入当前电脑的编译器/Python 路径；完成后重启 IDE。

GitHub 源码 ZIP、`git pull` 或单独 `SocAudio-0.3.0.zip` 不会自动更新已安装 core 的菜单、运行时或上传脚本。手动 core 优先于同名 Boards Manager core，请更新实际使用的副本。详见 [安装说明](INSTALL.md)。

## 验证范围与平台限制

- Windows x64 的标准 Boards Manager 安装及全部 25 个 KR260 示例编译通过；另已编译 ZedBoard 和 Python 构建钩子目标。WSL Linux x64 的标准安装及代表性音频示例编译通过；Windows 完整离线安装、配置保留和离线编译也已通过。
- Windows x64、WSL Linux x64 和 QEMU RV64 的内置 Python 已实际执行，确认 Python 3.12.16、pyserial 3.5 可以导入。
- RV64 主机 GCC/GDB 已在 QEMU RV64 下运行；通过 Arduino recipe 完整 clean 编译 `CourseDacSineIRQ` 和 `PcmClipPlayer`，输出 RV32IMC 软浮点程序，无 A/F/D 要求。另有生成的 C/C++ 程序在 `qemu-riscv32` 执行通过。尚未在原生 RV64 电脑验证完整安装/上传。
- Linux ARM64 和 Windows 11 ARM64 x64 仿真路径尚未作本次实机验证。其他 Linux 包需兼容其上游二进制的 glibc 环境，此处不声明未经验证的统一最低版本；不提供 32 bit Linux/ARM32 或 musl/Alpine 支持声明。
- 2026-11-17 已有 KR260 UART-over-SSH 自动启动及修正后 GDB 命令的实际硬件记录，上传后分别用独立串口观察确认应用持续执行。这不是本次各主机重新连接板卡的结果，见 [验证记录](VALIDATION.md)。呼吸灯外观已有用户确认；本机 COM 上传及模拟输出电压仍未实测。

完整离线包不包含 Arduino IDE、FPGA bitstream 或板端 OpenOCD/JTAG 服务。SSH 路径仍需要操作系统 OpenSSH 及已配置的板端环境；Flatpak/Snap 的文件系统、串口权限不会被安装器修改。工具和 Python 二进制仅作为 Release 资产分发，Git 保存源码、元数据及生成脚本。
