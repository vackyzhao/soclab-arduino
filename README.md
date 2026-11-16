# SoCLab Arduino — NEORV32 与 SocAudio

给 KIT SoC Lab 风格的 NEORV32 音频 SoC 使用的 Arduino core、板级包和 `SocAudio` 库。支持 GPIO、串口、定时及 48 kHz / 24 bit 立体声音频 FIFO；`Synth` 提供正弦波音源、多个声部和简单包络。

本包需要匹配的 FPGA SoC。它不能让普通 Arduino Uno 或一个未经配置的 KR260 自动获得音频外设，也不包含 FPGA bitstream。

## 直接在 Arduino IDE 安装

1. 在 **文件 → 首选项 → 附加开发板管理器网址** 中加入：

   ```text
   https://raw.githubusercontent.com/vackyzhao/soclab-arduino/main/package_soclab_index.json
   ```

2. 打开 **开发板管理器**，搜索 **KIT SoC Lab NEORV32**，安装本包。它会按电脑系统自动下载对应的 xPack RISC-V GCC/GDB；无需手动设置 PATH。
3. 在 **工具 → 开发板** 中选择目标，**Build** 选择 **Board (real time)**。
4. `SocAudio` 已随板级包提供：**文件 → 示例 → SocAudio**。

当前发布的 Boards Manager 工具依赖为 xPack **15.2.0-1**。索引使用明确版本及官方下载信息，保证安装器可正确选择并校验对应系统的文件。想主动更新到上游最新稳定工具链，可用下方脚本。

## 工具下载到 Arduino 工作目录

这是在学校 Linux 电脑、无 sudo 环境下也可使用的安装方式。先安装 Python 3、Arduino IDE 2 或 Arduino CLI，然后：

```sh
git clone https://github.com/vackyzhao/soclab-arduino.git
cd soclab-arduino
python3 tools/install_board.py
```

Windows PowerShell 使用：

```powershell
git clone https://github.com/vackyzhao/soclab-arduino.git
Set-Location soclab-arduino
python tools/install_board.py
```

脚本在安装或更新时查询官方 GitHub **latest stable release**，按系统及 CPU 架构下载工具，安装至：

```text
<Arduino sketchbook>/hardware/soclab/neorv32/
<Arduino sketchbook>/tools/soclab/xpack-riscv-none-elf-gcc/<版本>/
<Arduino sketchbook>/tools/soclab/downloads/
```

它会读取 Arduino IDE 2 的工作目录配置，也可手动指定：

```sh
python3 tools/install_board.py --sketchbook "$HOME/Arduino"
```

工具路径写入本机的 `platform.local.txt`，正常编译使用本地文件，不会每次联网。安装完成后重新打开 IDE。板卡管理器与手动安装二选一；同一 sketchbook 中的手动 core 会覆盖同名 Boards Manager core。

支持 Windows x64、Linux x64/ARM64、macOS Intel/Apple Silicon；当前工具的 Linux 版本要求 glibc ≥ 2.28。Windows 与 Linux x64 已实际编译验证，其他列出的主机使用官方资产映射，尚未实机验证。

离线及指定版本示例见 [安装说明](docs/INSTALL.md)。

## 单独安装 SocAudio 库

从 [Releases](https://github.com/vackyzhao/soclab-arduino/releases) 下载 `SocAudio-0.2.0.zip`，在 IDE 选择 **项目 → 加载库 → 添加 .ZIP 库**。

ZIP 中含标准 `library.properties`、`src/` 和 `examples/`。库依赖本仓库的 NEORV32 core 与 SoC 音频寄存器；安装 ZIP 不会安装 core 或配置 FPGA。

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

该程序由 CPU 实时生成正弦采样，经音频 FIFO 和 FPGA ΣΔ DAC 输出；不需要预先准备 WAV 文件。`Audio.write(left, right)` 可直接写入自己的有符号 24 bit 样本。详见 [SocAudio API 与示例](libraries/SocAudio/README.md)。

## 上传及调试

上传前目标电脑或 KR260 PS 上必须已有可用的 OpenOCD GDB server。普通 **上传** 的顺序是：

```text
target extended-remote <host>:3333
load
monitor reset run
detach
```

这样加载后会启动 CPU。`load` 本身只传程序，不能单独证明 CPU 已运行。该版本修正了旧上传流程缺少启动步骤的问题；KR260 r6 已实际通过远端上传、上传后自动运行及双向串口验证，见 [验证记录](docs/VALIDATION.md)。

- **Local**：本机 `127.0.0.1:3333`。
- **Remote**：在本机 `boards.local.txt` 中配置目标 IP；可填 Tailscale 地址。
- 串口是独立连接。选择远端 GDB 不会把 KR260 Linux 的 UART 自动转成 Windows 的 COM 口。

完整配置、FQBN、存储布局和硬件连接见 [上传与硬件说明](docs/UPLOAD.md)。

## 仓库结构

```text
hardware/soclab/neorv32/  标准 Arduino platform，含 core、variant、上游驱动及随附库
libraries/SocAudio/       可独立打 ZIP 的标准 Arduino 库
tools/install_board.py   跨系统安装／更新工具链到 sketchbook
tools/package_release.py 发布包生成脚本
package_soclab_index.json 开发板管理器索引
docs/                    安装、上传与验证记录
LICENSES/                上游许可及来源说明
```

## 许可与来源

本仓库保留 NEORV32 源文件的原有许可及署名。大部分上游驱动为 BSD-3-Clause；`neorv32_aux.c` 中特定日期转换函数带有独立的 GPL-2.0-or-later 许可，不能把整个分发包一概视为 BSD。原创 Arduino 适配层和 SocAudio 当前未另行授予开源许可证。详见 [第三方声明](LICENSES/README.md)。
