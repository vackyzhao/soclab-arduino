# 验证记录

日期：2026-11-16至2026-11-17。下述结果按验证层次区分；历史r6的GDB结果与新UART目标分别记录。

## 软件工具执行

- Windows x64 与 Linux x64 / glibc 2.39 已实际启动官方 xPack RISC-V GCC 15.2.0-1 和 GDB 16.3。
- Linux 上已实际编译 Blink 和 SinePolling，检查 ELF32 little-endian RISC-V、入口 `0xE0000000`，并用 GDB 反汇编启动代码。
- macOS Intel/Apple Silicon、Linux ARM64 在索引和安装器中配置官方对应资产，尚未在这些系统执行验证。

标准分发包已在 Windows 独立 data/user 目录通过 Arduino CLI 的 core install（自动工具依赖）及独立 SocAudio ZIP 安装；随后未指定 `compiler.path` 或修改 PATH，实际编译：

| 目标／示例 | 程序 | 静态 RAM | 结果 |
|---|---:|---:|---|
| ZedBoard / Blink | 3972 B | 668 B | 通过 |
| KR260 / SimpleTone | 8408 B | 2708 B | 通过 |

此轮从本机 HTTP 提供与 release 相同的板卡包，工具从官方资产缓存安装。Windows 深目录测试复现 GCC 的路径长度问题，头文件本身完整；换短 data 目录后通过，见安装说明。当前实际 Windows Arduino sketchbook 也已升级到 0.2.0，使用工作目录中的最新 GCC/GDB，并实际编译 SimpleTone 通过。

## GitHub 公开分发验证

公开发布后独立下载 raw 索引、板卡 tar.bz2 和 SocAudio ZIP，读回内容与上传的发布包一致。

Linux x64 的独立 data/user 目录从公开索引实际执行 `core install`，从 release 实际下载并安装 SocAudio ZIP。官方 Linux 编译器压缩包复用下载缓存，再由 CLI 按标准工具依赖正式安装。该环境没有手动 core、`platform.local.txt` 或 `compiler.path` 覆盖；PATH 只含 `/usr/bin:/bin`。

- ZedBoard 与 KR260 分别实际编译 Blink、SimpleTone，四项全部通过；大小与上表一致。
- 编译依赖文件确认使用独立 ZIP 安装到 user/libraries 的 SocAudio 0.2.0。
- 标准工具目录中的 GDB 16.3 实际读取 SimpleTone ELF，入口为 `0xE0000000`。

因此公开索引、平台下载、各主机工具依赖、独立库 ZIP 以及两种板型的正常编译流程已经过执行验证；macOS 和 Linux ARM64 仍未实机验证。

## 先前硬件逻辑仿真

KR260 clean Lab3 r6 与 r5b 的功能 RTL 相同；r6 改动音频输出管脚。先前执行的仿真覆盖：

- CPU Blink 与串口输出。
- 正弦采样 FIFO → ΣΔ DAC：无采样、密度、欠载或丢帧错误。
- 实际 RTL JTAG TAP：ID/DTMCS、halt、BRAM 写读、复位保留程序、恢复运行。
- 编译后的 Arduino SimpleTone：FIFO 输出连续、约 440 Hz，无欠载或丢帧。
- 实际 Xilinx AXI UART16550 与 CPU UART 双向收发、PS TX 源寄存器、双级 CDC、接收中断。

这些是数字逻辑仿真，不能代替实际板卡的模拟音频测量。最新 GCC 15.2 的编译结果也不等于这些新增镜像已经执行了同一轮 RTL 仿真。

## KR260 实板上传与串口

用户曾在 ZedBoard 上通过手动 GDB `run` 确认闪灯及 440 Hz 声音，这暴露了旧上传脚本只有加载而没有启动 CPU 的问题。本包普通上传现包含 `load → monitor reset run → detach`。

本轮已实际把 KR260 的 PL 切换为 clean Lab3 r6，Linux FPGA manager 为 `operating`。PS 上的 OpenOCD 0.12.0 经 AXI GPIO 桥实际识别 IDCODE `0x00000001` 和一个 RV32 hart；运行配置使用该版本支持的 `gdb_report_data_abort enable` 语法。

Windows Arduino CLI 使用本包 0.2.0、GCC 15.2.0-1 及 `soclab:neorv32:kr260:build=board,openocd=remote`，通过 Tailscale 向 PS 的 OpenOCD 上传串口／GPIO 测试程序：

- 程序 4052 B，静态 RAM 668 B；ELF 加载地址和入口均为 `0xE0000000`。
- 标准上传实际执行 `target extended-remote → load → monitor reset run → detach`，退出码为 0。
- 此后没有手动 `run`，也没有重新连接 GDB。PS 的 `/dev/ttyS0` 以 115200 baud 持续收到交替的 `LED=1`、`LED=0` 日志，并成功收到发给 CPU 的 `?` 回显。

因此该新版标准包在 KR260 r6 上的上传、上传后自动运行及 PS 与 CPU 双向 UART 已实际通过。这里的 GPIO 日志说明程序执行到对应操作；尚未独立观察 LED 外观或测量模拟音频。ZedBoard 的新版自动上传也尚未实板复核。

## KR260 原生 UART bootloader：2026-11-17

本轮实际部署`lab3_kr260_uart`，原r6 app仍保留。`xmutil loadapp`返回成功，独立`listapps`读回新app为active；FPGA manager为`operating`。实际设备树节点是`/sys/firmware/devicetree/base/axi/cpu-reset@a0030000`，reg解析为基址`0xA0030000`、范围`0x10000`，PS UART为`/dev/ttyS0`。`lab3-openocd`和`lab3-jtag`已停止，避免与UART reset争用共享MMIO锁。

使用Windows Arduino CLI的标准上传recipe，FQBN为`soclab:neorv32:kr260:build=board,uploadmethod=uartssh`，经SSH使用115200、8N1原生NEORV32 loader协议：

| 检查项 | 实际结果 |
| --- | --- |
| 示例 | `libraries/SocAudio/examples/BreathingLED/BreathingLED.ino`，UF1/GPIO20和UF2/GPIO21，500Hz软件PWM、4秒反向包络 |
| 编译 | 程序5208B，静态RAM680B，退出码0 |
| UART上传 | 发送5208个程序字节，loader接受长度和checksum |
| 自动启动 | loader的`e`跳到`0xE0000000`；无需GDB run |
| 独立持续运行检查 | 上传后仅读UART，不复位、不连接GDB；观察6秒，在0.866、1.866、2.866、3.866、4.866、5.866秒分别收到一个`.` |

以上证明原生UART上传、校验、自动启动和应用持续执行。随后用户反馈呼吸灯效果“正常”，作为人工外观确认；本轮没有示波器测量500Hz或模拟输出电压。

随后在同一`lab3_kr260_uart`目标启动匹配的JTAG桥与OpenOCD，Windows Arduino CLI以`soclab:neorv32:kr260:build=board,uploadmethod=gdb,openocd=remote`实际执行新GDB recipe：

- `monitor reset halt → load → monitor resume 0xE0000000 → detach`，程序5208字节，退出码0。
- detach后关闭调试连接，独立读取UART6秒，在0.253、1.253、2.253、3.253、4.253、5.253秒各收到一个`.`；没有另行reset或再次连接GDB。
- 日志中libgcc源码路径不存在的警告不影响程序装载、启动或后续运行。

新KR260 GDB recipe因此也已通过实板上传及继续执行检查。之后停止OpenOCD/JTAG服务，切回UART模式。本机COM上传路径、500Hz波形及`analogWrite()`模拟电平仍未实板测量；LED外观有用户人工确认。

本地工作区证据位于`outputs/kr260_uart_bootloader_20261117/`：

- `board_deployment.log`：服务停止、app切换、独立active/operating与DT寄存器读回。
- `breathing_build/command.json`、`compile.log`、`compile_result.json`：实际FQBN、构建命令与编译结果。
- `breathing_uart_upload.log`：长度/checksum确认及启动入口。
- `breathing_uart_running.json`：6秒独立串口事件与未reset/未接调试器状态。
- `breathing_gdb_upload.log`：新GDB recipe实际装载5208字节并detach。
- `breathing_gdb_running.json`：GDB上传后6秒独立串口事件与未reset/未接调试器状态。

以上为2026-11-17的硬件记录；UART菜单和上传工具现纳入0.3.0发行版，当前主机工具验证见下文。

## 0.3.0 主机工具与完整离线安装：2026-11-23

本轮使用 Arduino CLI 1.6.0，先从本地 HTTP 提供待发布索引及原样发行资产，在独立 data/user 目录执行标准 Boards Manager 安装。没有访问或烧写 FPGA，`build=sim` 的检查仅执行 Python 构建钩子和编译，不是重新进行 RTL 仿真。

| 环境 | 实际执行 | 结果 |
| --- | --- | --- |
| Windows x64 / Boards Manager | 安装 0.3.0 和两个主机工具依赖；KR260 全部 25 个示例；ZedBoard `SimpleTone`；KR260 `build=sim` 的 `Blink` | 全部通过 |
| WSL Linux x64 / Boards Manager | 安装 0.3.0；KR260 `AnalogWriteStereo`、`CourseDacSineIRQ`、`PcmClipPlayer`；ZedBoard `SimpleTone`；`build=sim` 的 `Blink` | 全部通过 |
| Windows x64 / 完整离线安装器 | 保留自定义连接配置，安装 GCC/GDB/Python；移除第三方 PATH 依赖，运行工具；阻断 HTTP/HTTPS 代理后编译 `AnalogWriteStereo` | 通过，程序 4832 B，静态 RAM 400 B |
| RV64 原生主机工具 / QEMU RV64 | GCC/GDB 启动；Arduino CLI 调用原生 RV64 编译器，分别 `--clean` 编译 `CourseDacSineIRQ` 和 `PcmClipPlayer` | 通过，程序 5112 B / 11292 B，静态 RAM 1320 B / 8080 B |
| 内置 Python | Windows x64、Linux x64、QEMU RV64 执行 Python 3.12.16，导入 pyserial 3.5；x64 同时检查 ssl/sqlite3/ctypes/zlib | 通过 |

RV64 构建由 Linux x64 Arduino CLI 调用 QEMU 下的 RV64 工具完成；这验证原生工具与 Arduino recipe 的配合，不等于在 RV64 实机上运行 IDE、安装或上传。两个输出均为 RV32IMC 软浮点 ELF，入口 `0xE0000000`，没有 A/F/D 扩展要求；另有 C/C++ 目标程序在 `qemu-riscv32` 执行通过。

安装器、Python 运行时打包和 UART 协议共 29 项 Python 测试通过。四个完整离线 ZIP 均包含各自主机的工具链、Python/pyserial、25 个示例及许可声明，已检查 ZIP 内容和完整性。Linux Python 工具归档显式保留父目录，以兼容 Arduino 工具安装器的符号链接解压。

Windows 深目录测试复现上游 GCC 的长路径头文件查找问题，改用短安装目录后编译通过。Linux ARM64 的二进制架构和分发元数据已经核对，尚未在 ARM64 实机执行；Windows 11 ARM64 使用 x64 仿真路径，也尚未作本轮实机验证。新发布包不代表新增硬件测量结果。

本地完整日志：`outputs/arduino_release_20261123/`；Windows 25 示例记录另位于 `C:/Temp/soclab030-win/results.json`。Linux 记录位于 `manager_linux_runtime_fix/`（日志内实际 WSL 安装目录为 `manager_linux`），RV64 记录为 `work/rv_arduino_validation.json`，离线安装记录为 `offline_windows_test/`。
