# 课程预编译 ELF 的 Arduino 重建

这三个示例根据 Lab 3 的 `blinky.elf`、`dac_sin.elf`、`dac_sin_ir.elf`
反汇编、符号和初始化数据重建行为。它们是可读的 Arduino 移植，不能称为老师的原始源码。
ELF 保留了函数名和 DWARF 信息，但原始注释、表达式写法及局部变量名不能完整恢复。

| Arduino 示例 | 原文件 | 实际行为 |
| --- | --- | --- |
| `CourseBlinky` | `blinky.elf` | GPIO20、21 同时置高；每 250 ms 同步翻转，完整周期 500 ms。 |
| `CourseDacSine` | `dac_sin.elf` | 轮询 FIFO，109 点循环正弦同时送到左右声道；每写完 256 个正弦周期翻转 GPIO20。 |
| `CourseDacSineIRQ` | `dac_sin_ir.elf` | FIFO 低阈值 128，GPIO22 上升沿中断设置标志；主循环补音频，消费标志累计 256 次翻转 GPIO20。 |

两个音频程序使用相同的 109 点查表数据，48 kHz 播放，所以频率是
`48000 / 109 = 440.366972... Hz`。它们没有读取 WAV 文件，也没有 ADSR、混音等合成器功能。
表的幅度接近有符号 24 bit 满幅，与 `Synth` 按声部数量缩放幅度的规则不同。

## 数据还原与接口

原 ELF 的 `sin_buf` 在 `.data`，地址 `0x80000000`，共 436 字节。
两个文件的表逐字相同，所有 109 个采样均已提取。
负半周在原文件中保存为 **24 bit 补码，高 8 bit 为零**，例如最后一个字
`0x00F8A02C` 表示 `-483284`。

`Audio.write(int32_t)` 会按有符号数限幅，因此这里先将这些值转换成有符号
`int32_t`。两个 `CourseSineTable.h` 的低 24 bit 与原 ELF 完全相同。
表保留为可写全局变量，放入 DMEM；原 ZedBoard XIP 平台可避免查表与取指争用缓存。

音频外设接口保持为 `0xFFD00000`：

| 偏移 | 含义 |
| --- | --- |
| `+0x00` | CTRL：bit0 FIFO reset，bit1 enable；启动写入 `3 → 2`。 |
| `+0x04` | STAT：bit2 表示 FIFO full。 |
| `+0x08` | FIFO low 阈值；IRQ 示例设为 128。 |
| `+0x0C` | FIFO 当前帧数。 |
| `+0x10` | 左声道采样。 |
| `+0x14` | 右声道采样；bit31 提交完整双声道帧。 |

`Audio.write()` 完成成对写入并检查 FIFO 是否满，只有成功后才推进表索引。
重建代码使用这套现有库接口，不复制 ELF 中的启动代码或固定链接地址。

## 中断版的准确流程

1. 初始化 DAC，设置 128 帧低水位，注册 GPIO22 `RISING` 中断。
2. 首次 FIFO 未满，主循环直接写入采样，无需等待首次中断。
3. FIFO 满后忙等 `fifoLowIr` 标志；不是 `WFI` 休眠。
4. FIFO 播放至低于 128 帧时，GPIO22 出现上升沿。
5. Arduino GPIO 分发器清 pending，然后回调只执行 `fifoLowIr = true`。
6. 主循环清标志并继续补 FIFO。表索引连续，不从零重新播放。

原程序的 8 bit 计数器按**主循环消费标志**计数；多个中断可能合并成同一个标志，
不能把它描述为精确统计 ISR 调用次数。不要同时调用 `Synth.begin()` 或 `tone()`，
它们会使用同一音频 FIFO 和 GPIO22 中断。

## Arduino 使用

安装本次修改后的 SoCLab 板级包后，在 **文件 → 示例 → SocAudio** 中选择以上示例。
也可以直接打开各示例文件夹中同名的 `.ino`；两个音频示例需要保留同目录的
`CourseSineTable.h`。

KR260 使用 **KIT SoC Lab - KR260 + NEORV32**，Build 选 **Board (real time)**，
OpenOCD target 按实际目标选择。UART 速率为 19200。
GPIO20/21 对应的实际灯和音频引脚由 FPGA 顶层及约束决定；代码不重新配置 FPGA 引脚。

Arduino core 接管 trap/启动流程，`Serial.begin()` 自带 RX 支持，
`attachInterrupt()` 接管 GPIO pending 清除，因此这些实现细节与原 ELF 不逐指令相同。
闪灯示例先检查 GPIO 可用性，再写输出；原 ELF 在检查前还写过一次 GPIO20。
两个音频示例的 PCM 序列、阈值、标志驱动流程和计数单位均按 ELF 还原。

## 验证

本次编译、ELF 表比对和实际 CPU/音频 RTL 仿真的结果记录在工作区
`outputs/soclab_20261026/precompiled_to_arduino_20261116/`。
Simulation 构建会将 UART 改为 1228800 baud、加速软件延时；硬件音频仍为 48 kHz。
编译与 RTL 仿真不代表已在板上重新加载或测量实际模拟音频。
