# SoCLab Arduino — NEORV32 与 SocAudio

给 KIT SoC Lab 风格的 NEORV32 音频 SoC 使用的 Arduino core、板级包和 `SocAudio` 库。支持 GPIO、串口、定时及 48 kHz / 24 bit 立体声音频 FIFO；`Synth` 提供正弦波音源、多个声部和简单包络。

**0.3.0** 提供 25 个示例、经现有 ΣΔ DAC 输出的 `analogWrite()`、UART 自动上传并启动，以及修正后的 KR260 GDB 启动命令。Windows/Linux 完整离线包带有 GCC/GDB、CPython **3.12.16** 和 pyserial **3.5**，安装及日常编译不依赖系统 Python 或手动设置 PATH。详见 [0.3.0 发布说明](docs/RELEASE_NOTES_0.3.0.md)。

本包需要匹配的 FPGA SoC，不包含 FPGA bitstream、Arduino IDE 或板端 OpenOCD 服务。

## Arduino IDE 安装或更新

1. 在 **文件 → 首选项 → 附加开发板管理器网址** 中加入：

   ```text
   https://raw.githubusercontent.com/vackyzhao/soclab-arduino/main/package_soclab_index.json
   ```

2. 打开 **开发板管理器**，搜索 **KIT SoC Lab NEORV32**，选择 **0.3.0** 并安装或直接更新；无需先删除旧版。管理器按主机安装对应 GCC/GDB 和 Python 运行时。
3. 在 **工具 → 开发板** 中选择目标，**Build** 选择 **Board (real time)**。
4. 从 **文件 → 示例 → SocAudio** 打开示例。库已随板包提供，无需再安装库 ZIP。

如果之前是手动安装到 `<sketchbook>/hardware/soclab/neorv32`，该 core 会优先于 Boards Manager 的同名 core。请按下节使用完整离线安装器更新原目录；仅更新管理器中的另一份 core 不会更新正在使用的手动副本。

## 完整离线安装：无需 sudo、系统 Python 或 PATH 配置

从 [0.3.0 Releases](https://github.com/vackyzhao/soclab-arduino/releases/tag/v0.3.0) 下载对应电脑的完整包：

| 电脑系统 | 完整离线 ZIP |
| --- | --- |
| Windows x64 | `SoCLab-Arduino-0.3.0-offline-x86_64-mingw32.zip` |
| Windows 11 ARM64 | 使用上述 Windows x64 包，经 x64 仿真运行；不是原生 ARM64 工具链 |
| Linux x64 | `SoCLab-Arduino-0.3.0-offline-x86_64-linux-gnu.zip` |
| Linux ARM64 | `SoCLab-Arduino-0.3.0-offline-aarch64-linux-gnu.zip` |
| Linux RISC-V64 | `SoCLab-Arduino-0.3.0-offline-riscv64-linux-gnu.zip`；全套 Arduino 示例验证尚未完成 |

完整解压 ZIP，保留其中的 `hardware/`、`tools/`、`toolchains/` 和 `runtimes/`。Windows 双击 `install.cmd`；也可在 PowerShell 指定实际 sketchbook：

```powershell
.\install.cmd --sketchbook "D:\Arduino"
```

Linux 在解压目录执行：

```sh
sh install.sh --sketchbook "$HOME/Arduino"
```

以上路径只是示例，须与 **Arduino IDE → 首选项 → 项目文件夹位置 / Sketchbook location** 一致。安装器把工具和 Python 保存在该目录，写入绝对路径，保留 `boards.local.txt` 及无关的 `platform.local.txt` 设置；然后完全关闭并重开 IDE。安装过程不需要联网、sudo、系统 Python 或修改 PATH。

Linux 需要兼容的 glibc 环境；RISC-V64 工具要求 **glibc ≥ 2.38**。本次不提供 32 bit Linux/ARM32 或 musl/Alpine 支持声明。RISC-V64 包组合 RISCstar GCC 15.1/GDB 16.3 主机工具与 xPack 的 RV32I/ILP32 目标库：已在 QEMU 下运行 GCC/GDB 及 RV32I C/C++ 程序，但尚未完成 RV64 主机上的全套 Arduino 示例或原生硬件验证。Windows 11 ARM64 的 x64 仿真路径也未作实机验证。

Flatpak/Snap 版 IDE 仍需能访问所选 sketchbook 和实际串口；安装器不更改 IDE 的文件系统或串口权限。UART-over-SSH 仍需要操作系统提供 OpenSSH `ssh`。详细更新步骤和源码安装方式见 [安装说明](docs/INSTALL.md)。

**GitHub 源码 ZIP、`git pull` 或 `SocAudio-0.3.0.zip` 本身不会更新已安装的 core、菜单或上传脚本。** 单独库 ZIP 适用于已有匹配 core 的用户；在 IDE 选择 **项目 → 加载库 → 添加 .ZIP 库**，不能替代板包更新。

## 最小音频程序

```cpp
#include <SocAudio.h>

void setup() {
  Synth.begin();
  Synth.setVolume(128);     // 0..255
  Synth.noteOnHz(0, 440.0f);
}

void loop() {}
```

CPU 实时生成正弦采样，经音频 FIFO 和 FPGA ΣΔ DAC 输出；不需要预先准备 WAV 文件。`Audio.write(left, right)` 可直接写入自己的有符号 24 bit 样本。详见 [SocAudio API](libraries/SocAudio/README.md)。

`analogWrite(DAC_LEFT, value)` 和 `analogWrite(DAC_RIGHT, value)` 使用同一音频外设产生两个独立的平均输出电平；需包含 `<SocAudio.h>`。`analogWriteResolution()` 支持 1–24 bit，默认 8 bit。普通 GPIO 的 `analogWrite()` 仍按阈值输出 HIGH/LOW，不会变成硬件 PWM。模拟电压需经合适的低通滤波，音频交流耦合输出也不能用于直流测量。详见 [analogWrite 说明](libraries/SocAudio/ANALOG_WRITE.md)。

25 个示例覆盖 GPIO、非阻塞计时、串口命令、左右声道、自定义波形、PCM 片段、旋律、滑音、多声部和 ΣΔ DAC 平均电平。完整学习顺序、接线和运行说明见 [中文示例导览](libraries/SocAudio/EXAMPLES.md)。

## 上传及调试

KR260 在 **工具 → Upload method** 选择：

| 方式 | 连接路径 |
| --- | --- |
| **GDB / OpenOCD**（`gdb`） | 本机或远端 OpenOCD；Remote 可配置局域网/Tailscale 地址 |
| **UART 115200 through KR260 SSH**（`uartssh`） | 经操作系统 SSH 和 PS 的 `/dev/ttyS0` 上传，无需本机 COM 口 |
| **UART 115200 through local serial port**（`uartlocal`） | 随包 Python/pyserial 经所选 COM/tty 上传 |

KR260 GDB 上传顺序为：

```text
target extended-remote <host>:3333
monitor reset halt
load
monitor resume 0xE0000000
detach
```

先复位停住 CPU，再装载并从 BRAM 应用入口启动。带独立 loader 的 KR260 在复位后进入 boot ROM，不能用 `reset run` 代替启动刚装入的应用。ZedBoard 的 XIP 路径仍使用 `load → monitor reset run → detach`，完成 flash 启动初始化。

UART 自动发送原生 `u` 命令、上传 BIN、核对 `OK`，再用 `e` 启动；**无需额外手动 GDB run**。它需要匹配的 `lab3_kr260_uart` bitstream/设备树和 loader。SSH 路径还需独立 CPU reset GPIO `0xA0030000`；切到 UART 前须停止共用 MMIO 锁的 `lab3-openocd`、`lab3-jtag` 服务。板包安装不会自动配置这些板端条件。

2026-11-17 的实际 KR260 验证已覆盖 UART-over-SSH 自动启动和新 GDB 命令，上传后分别通过独立 6 秒串口观察确认应用持续输出。呼吸灯外观已有用户确认；本机 COM 上传和模拟输出电压尚未实测。该记录不代表本次所有主机都完成了硬件测试。见 [上传与硬件说明](docs/UPLOAD.md) 和 [验证记录](docs/VALIDATION.md)。

## 仓库结构

```text
hardware/soclab/neorv32/    Arduino platform、core、variant、驱动及随附库
libraries/SocAudio/         可单独打 ZIP 的 Arduino 库及 25 个示例
tools/install_board.py      安装/更新到指定 sketchbook
tools/package_offline.py    生成带工具链和 Python 的完整离线包
tools/package_python_runtime.py  打包完整 CPython 与 pyserial
tools/package_release.py    生成板包、库包和管理器索引
package_soclab_index.json   开发板管理器索引
docs/                      安装、上传、API、发布和验证说明
LICENSES/                  上游许可及来源说明
```

## 许可与来源

本仓库保留 NEORV32 源文件的原有许可及署名。多数上游驱动为 BSD-3-Clause；`neorv32_aux.c` 中特定日期转换函数带独立 GPL-2.0-or-later 声明，不能把整个分发包一概视为 BSD。原创 Arduino 适配层和 SocAudio 当前未另行授予开源许可证。

编译工具和 Python 二进制保存在 Release 资产中，不提交 Git。完整包保留 CPython、pyserial、工具链及其依赖的许可和源码来源说明。详见 [第三方声明](LICENSES/README.md)。
