# 验证记录

日期：2026-11-16。下述结果按验证层次区分。

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

## 实板边界

用户曾在 ZedBoard 上通过手动 GDB `run` 确认闪灯及 440 Hz 声音，这暴露了旧上传脚本只有加载而没有启动 CPU 的问题。本包普通上传现包含 `load → monitor reset run → detach`。

当前记录尚未把该新版标准包的自动上传、KR260 r6 编程后运行、LED 外观或模拟音频实测记为已通过。FPGA r6 的实现报告通过时序分析，但它与实际板卡程序运行属于不同层次的证据。
