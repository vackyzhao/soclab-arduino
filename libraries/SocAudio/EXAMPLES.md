# SocAudio 示例导览

在 Arduino IDE 打开 **文件 → 示例 → SocAudio**。开发板选择实际使用的 KR260 或课程 ZedBoard，Build 选择 **Board (real time)**。

这些例子使用 Lab3 音频 SoC 的外设布局；普通 Arduino Uno 不适用。

## 推荐顺序

1. **Blink → BlinkWithoutDelay → BreathingLED**：确认 CPU 启动及 LED 映射，再看非阻塞计时和软件 PWM。
2. **SerialHello → SerialCommandConsole → TimingAndRandom**：串口、命令解析和当前 core 支持的通用函数。
3. **SimpleTone → StereoChannelTest → WaveformPlayer**：先听单音，再检查左右声道，最后自己提供采样值。
4. **PcmClipPlayer**：播放预先生成的有限 PCM 片段，理解采样播放与实时音符合成的区别。
5. **NonBlockingMelody → PitchGlide → SynthSerialControl → Chord**：旋律、连续滑音、实时控制和多声部。
6. **AnalogWriteStereo → DacResolution**：研究同一 ΣΔ 输出的静态平均电平。

## GPIO、串口和时间

| 示例 | 主要内容 | 预期效果 |
| --- | --- | --- |
| [Blink](examples/Blink/Blink.ino) | GPIO、delay | 最基础的 LED 状态切换。 |
| [BlinkWithoutDelay](examples/BlinkWithoutDelay/README.md) | millis 差值计时 | 两颗 LED 分别每 500 / 700 ms 翻转。 |
| [BreathingLED](examples/BreathingLED/README.md) | micros、软件 PWM | 两颗灯反向呼吸，完整周期 4 秒。 |
| [SerialHello](examples/SerialHello/SerialHello.ino) | Serial、Print 格式化 | 串口输出及输入回显；波特率以源码为准。 |
| [SerialCommandConsole](examples/SerialCommandConsole/README.md) | 固定缓冲区、CR/LF | 115200 波特命令行控制两颗 LED，输入 help 查看命令。 |
| [TimingAndRandom](examples/TimingAndRandom/README.md) | 时间、随机数、map、位操作 | 115200 波特打印可重复的随机序列和计时结果。 |

KR260 clean Lab3 / lab3_kr260_uart 中，**GPIO20 = UF1，GPIO21 = UF2**。课程 ZedBoard 中它们对应扩展板 D2 / D3。灯的位置由实际 FPGA 约束决定。

Serial 是 **NEORV32 的 UART**。KR260 经 PS 的 /dev/ttyS0 访问；OpenOCD 的 TCP 3333 端口用于 GDB，不能当作串口终端。新串口示例用 115200；旧 SerialSynth 压力示例用 19200。上传时关闭占用该 UART 的终端。

millis / micros 是 32 位无符号计数，使用 now - last >= interval 处理回绕。cycles 只返回周期计数低 32 位，在 98.304 MHz 下约每 43.7 秒回绕；短时间差可以相减，不能直接统计任意长的运行时间。

## 音频数据与输出

| 示例 | 声音来源 | 重点 |
| --- | --- | --- |
| [SimpleTone](examples/SimpleTone/SimpleTone.ino) | Synth 实时正弦 | 最少代码输出 440 Hz。 |
| [StereoChannelTest](examples/StereoChannelTest/README.md) | 自己产生左右 PCM | 左 440 Hz、右 660 Hz、双声道、静音轮换。 |
| [WaveformPlayer](examples/WaveformPlayer/README.md) | 自己计算采样 | 正弦、三角、锯齿、方波轮换；后三种未做带限处理。 |
| [PcmClipPlayer](examples/PcmClipPlayer/README.md) | 预先生成的 16 bit PCM | 有限音频数组用 Audio.write16 播放，再插入静音；附 Python 生成器。 |
| [SinePolling](examples/SinePolling/SinePolling.ino) | 循环读取 109 点正弦表 | 直接写 FIFO，约 440.37 Hz。 |
| [NonBlockingMelody](examples/NonBlockingMelody/README.md) | Synth 音符 | millis 调度音符与休止，主循环可处理其他任务。 |
| [PitchGlide](examples/PitchGlide/README.md) | Synth 连续改频率 | 220 ↔ 880 Hz 滑音，setFrequency 保持当前包络。 |
| [SynthSerialControl](examples/SynthSerialControl/README.md) | Synth 加串口控制 | 115200；1–8 音符、+ / - 音量、空格停止、? 状态。 |
| [ToneScale](examples/ToneScale/ToneScale.ino) | Synth 音阶 | 音符时序及合成统计。 |
| [Chord](examples/Chord/Chord.ino) | 多个正弦声部相加 | 选择 Synth voices: 4 或更多。 |
| [FullScale](examples/FullScale/FullScale.ino) | 两声部幅度测试 | 选择 Synth voices: 2，观察声部数与幅度。 |

PcmClipPlayer 的内置声音由附带脚本数学生成，不是录音，也不是外部文件实时解码。它存的是一整段有限采样；WaveformPlayer 则运行时连续生成周期波形。要换成自己的录音，先在电脑上转成例子指定的 **48 kHz / 有符号 16 bit / 单声道**数组，再替换资产；当前 core 没有文件系统或 MP3/WAV 解码器。

KR260 匹配的 Lab3 bitstream 使用 **PMOD1 / J2：物理脚 1 = 左，3 = 右，9 / 10 = GND**。它们输出 3.3 V 逻辑的 ΣΔ 位流，接原有低通滤波、耦合和放大链路。ZedBoard 使用课程扩展板连接。数字输出正确、滤波后电压和实际声音需分别验证。

## FIFO 与主循环

外设为 **48 kHz / 有符号 24 bit / 双声道 / 256 帧 FIFO**。`Audio.write(left, right)` 返回 1 才表示接受一帧；返回 0 时保留当前采样位置，下次再写。`Audio.write16(left, right)` 把 16 bit 扩展至 24 bit。

直接 PCM 例子按**成功写入的帧数**推进状态，播放期间不长时间 delay 或大量打印。FIFO 全满也只够约 **5.33 ms**。ZedBoard 从 XIP flash 运行且缓存较小，因此播放时频繁访问的采样表放在 DMEM；PcmClipPlayer 初始化时把短片段复制到 RAM，受 16 KiB DMEM 总量限制。

Synth 默认用 GPIO22 FIFO-low 中断补数据。它与 Audio 共用 FIFO，应选择一种写入方式。`Synth.begin()`、`Audio.begin()` 与 `analogWrite(DAC_*, ...)` 会切换模式；不要播放中反复初始化。主循环串口任务可与中断合成并行，关中断太久仍会影响音频。

默认 **Synth voices: 4**。只用 voice 0 的新增合成例子也支持选择 1；声部数由板菜单编译进库，不能只在 ino 内 define。库为所有配置声部分配幅度余量，所以声部数不同，单音响度也可能不同。

## 静态 DAC 与进阶观察

| 示例 | 内容 |
| --- | --- |
| [AnalogWriteStereo](examples/AnalogWriteStereo/AnalogWriteStereo.ino) | 8 bit 互补左右平均电平，每秒切换。 |
| [DacResolution](examples/DacResolution/README.md) | 8 / 12 / 24 bit，0%、25%、50%、75%、100% 互补电平，每档 2 秒。 |
| [PinInterrupt](examples/PinInterrupt/PinInterrupt.ino) | 内部 GPIO22 FIFO-low 中断模式，有意写入静音。 |
| [SynthBench](examples/SynthBench/SynthBench.ino) | 合成渲染开销与声部数。 |
| [SerialSynth](examples/SerialSynth/SerialSynth.ino) | 合成同时进行 UART 收发的压力示例。 |
| [CourseBlinky](examples/CourseBlinky/CourseBlinky.ino)、[CourseDacSine](examples/CourseDacSine/CourseDacSine.ino)、[CourseDacSineIRQ](examples/CourseDacSineIRQ/CourseDacSineIRQ.ino) | 按课程预编译 ELF 的可观察行为重写，见 [来源说明](COURSE_ELF.md)。 |

`analogWriteResolution()` 设置**数值精度**，不表示电路实际达到相同有效位数。DAC_LEFT / DAC_RIGHT 支持 ΣΔ 平均电平，普通 GPIO 的 analogWrite 当前只是半量程阈值 HIGH / LOW，呼吸灯因此使用软件 PWM。测 DC 应在低通后、隔直电容前观察，电容后的交流输出不传递静态直流。详见 [模拟输出说明](ANALOG_WRITE.md)。

## 安装与验证范围

板包随附 SocAudio，无需再装同名 ZIP。仓库主副本在 `libraries/SocAudio/`，随板包副本在 `hardware/soclab/neorv32/libraries/SocAudio/`。更新本机后若菜单仍旧，可重启 Arduino IDE。

新增示例属于开发版，已有公开 0.2.0 ZIP 不会自动获得这些文件。跨系统安装见 [安装说明](https://github.com/vackyzhao/soclab-arduino/blob/main/docs/INSTALL.md)，连接见 [上传说明](https://github.com/vackyzhao/soclab-arduino/blob/main/docs/UPLOAD.md)。ino 与目录同名，可直接打开。

编译覆盖 KR260 BRAM 与 ZedBoard XIP 的 Board 模式；各例的实际听感、示波器测量及串口交互仍需在对应硬件上验证。说明中的预期效果不表示已逐个烧录实测。

2026-11-17，新增 10 例完成上述两板型共 20 项编译。另将原样 PcmClipPlayer 固件运行于真实 NEORV32 CPU 与 Lab3 音频 RTL 的仿真：完整 3840 帧片段及随后 128 帧静音在 DAC 消费端逐样本匹配，FIFO 填充后无饥饿。该代表测试覆盖约 83.234 ms 的直接 BRAM 启动和数字播放，不覆盖完整一秒静音后的再次播放、UART 上传或模拟声音。
