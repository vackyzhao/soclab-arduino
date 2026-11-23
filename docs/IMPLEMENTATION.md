# 这套 NEORV32 Arduino 的实现

本文依据本仓库代码及 KR260 clean Lab3 r6 的实际 RTL 配置说明。
UART bootloader 新配置仍在单独验证，不能把主机脚本存在当成板上已经支持。

## 1. 各层分别负责什么

```text
你的 .ino：setup / loop / Serial / Audio / Synth
               │
Arduino core：入口、时间、GPIO、串口缓冲、中断分发
               │
NEORV32 官方 C 库：寄存器、CPU CSR、GPIO/UART 驱动、RTE
               │
NEORV32 CPU + GPIO/UART + 自定义音频 MMIO
               │
FPGA 中的 FIFO、ΣΔ DAC、板级引脚
```

Arduino IDE 是电脑上的编辑、构建及上传入口。板子执行的是 RISC-V 机器码。
工程没有在 CPU 上运行一个解释 `.ino` 的解释器，也没有为此增加 Linux 或 RTOS。
KR260 的 PS Linux 负责开发连接与桥接；执行 sketch 的是 PL 中的 NEORV32。

`system/neorv32/sw/lib/` 是上游软件库；`cores/neorv32/` 是本工程的 Arduino 风格适配层。
不是整套照搬 ArduinoCore-API，因此这里只能承诺代码中实际实现的接口。

## 2. 程序如何进入 setup 和 loop

源文件：`hardware/soclab/neorv32/cores/neorv32/main.cpp`，上游 `system/neorv32/sw/common/crt0.S`。

链接入口是 `_start`。启动汇编先准备寄存器、栈和 C/C++ 运行环境，
把带初值的 `.data` 从程序镜像复制到 DMEM，清零 `.bss`，执行全局 C++ 构造函数，
随后调用 `main()`。

Arduino 的 `main()` 保存构造函数已启用的中断源，初始化 NEORV32 RTE，
将 `mtvec` 换成 Arduino 的 DMEM 中断入口，恢复中断源并开全局中断，随后执行：

```cpp
setup();
for (;;) {
    loop();
}
```

所以 `setup()` 只调用一次，`loop()` 返回后立即再次调用。用户代码无须再写一个 `main()`。

## 3. pinMode、digitalWrite 和灯

源文件：`cores/neorv32/wiring.cpp`。

这个 Lab3 GPIO 的输入、输出线在 RTL 中分别固定连接，没有普通 MCU 的方向寄存器。
`pinMode(pin, OUTPUT)` 只在 core 的 `output_pins` 位图中记录模式：
之后 `digitalRead()` 对 OUTPUT 读取输出锁存器，对 INPUT 读取输入端口。
它不会把任意 FPGA 引脚动态变成输入或输出；`INPUT_PULLUP` 也不能凭空产生上拉电阻。

`digitalWrite(20, HIGH)` 的核心动作是：

```cpp
NEORV32_GPIO->PORT_OUT |= 1u << 20;
```

实际代码会检查 pin 范围，并在这个读—改—写期间暂时关闭中断、随后恢复原状态，
避免中断处理函数同时改同一个寄存器而丢掉别的位。
`digitalToggle(20)` 则对第 20 位异或。

在当前 KR260 r6 约束中，GPIO20 对应 UF1，GPIO21 对应 UF2。
Arduino 的数字 20 是 SoC GPIO 位号，不是排针第 20 脚。
板包仍沿用了 `zedboard_soc` variant 名和部分 ZedBoard 注释，物理连接应以目标 RTL/XDC 为准。

## 4. delay、millis、micros

同样在 `wiring.cpp`。它们读取 RISC-V 的 64 位 `mcycle`，并按 `F_CPU=98304000` 换算时间。
读取顺序是高 32 位、低 32 位、再读高 32 位；如果高位变化就重读，避免跨溢出读错。

`delay(500)` 计算目标周期数，在循环里等待计数器到达目标，属于忙等。
它没有让 CPU 休眠。循环期间中断仍可运行，也会调用 idle hook 和 `yield()`；
轮询模式的 Synth 利用这个 hook 在等待时继续补音频。
`delayMicroseconds()` 是更短的周期等待，不调用该补帧 hook。

Simulation 配置可以把软件时间放大 1000 倍，以缩短验证时间；
例如代码中的 250 ms 变成实际仿真 250 us。音频 DAC 的 48 kHz 不随它缩放。

## 5. Serial

源文件：`cores/neorv32/HardwareSerial.cpp`、`Print.cpp`。

`Serial.begin(115200)` 调用上游 `neorv32_uart0_setup()`，配置 UART0 波特率与接收中断。
TX 路径将字节写入 UART0 DATA；TX FIFO 满时等待，并调用 idle hook。
`Serial.print(123)` 的数值转文本由 `Print` 层完成，然后交给这个字节发送函数。

RX 路径由中断把硬件 FIFO 中的字节搬到 DMEM 环形缓冲，
`available()` 查看数量，`read()` 取出一个字节。代码还记录软件缓冲溢出和硬件 overrun。
长音频中断会临时关中断，因此音频补帧每完成一块就主动检查 UART RX，降低漏收概率。
当前 `begin(baud, config)` 会忽略 `config`，不能据此宣称所有 Arduino 串口帧格式都已实现。

KR260 的数据路径是：

```text
NEORV32 UART0 → PL 串口跨时钟同步 → PS AXI16550 → Linux /dev/ttyS0
```

Tailscale/SSH 是电脑访问 PS 的路径，不会自动生成 Windows COM 口。
同样，OpenOCD 的 3333 端口是调试连接，和这个 UART 是两条不同的连接。

## 6. attachInterrupt 和 RAMFUNC

源文件：`cores/neorv32/trap.c`、`wiring.cpp`。

GPIO 各位共用一个 NEORV32 FIRQ。`attachInterrupt()` 保存回调指针、设置触发方式，
再启用外设与 CPU 对应中断源。中断发生后：

1. 汇编入口保存调用者寄存器和 `mepc`。
2. 根据 `mcause` 查找中断分发表。
3. GPIO 分发器读取并确认 pending 位，调用相应 pin 的回调。
4. 恢复寄存器，执行 `mret` 回到被打断的位置。

中断处理期间全局中断关闭，没有嵌套中断。
`RAMFUNC` 把函数放到 `.data.ramfunc`，由启动代码一起复制到 DMEM 执行。
这对 ZedBoard 的 XIP 程序尤其重要，可以避免关键中断路径等待 SPI cache refill；
KR260 程序本来就在 BRAM，但仍保留这套布局，方便两种目标共用软件。

## 7. Audio 是寄存器驱动

源文件：`libraries/SocAudio/src/SocAudio.cpp`、`SocAudio.h`。

| 偏移（基址 `0xFFD00000`） | 内容 |
| --- | --- |
| `0x00` | 控制：bit0 reset、bit1 enable |
| `0x04` | FIFO low / empty / full 状态 |
| `0x08` | FIFO 低水位阈值 |
| `0x0C` | 当前帧数 |
| `0x10` | 左声道样本暂存 |
| `0x14` | 右声道样本，bit31 同时提交一帧 |

`Audio.begin(128)` 先写控制值 3 复位并使能，再写 2 解除复位，然后设低水位 128。
`Audio.write(left, right)` 先把两路输入限制在有符号 24 bit 范围，保留低 24 位；
短暂关闭中断、检查 full、写 L、写带 commit 的 R，最后恢复中断。
FIFO 满时返回 0，不写入；成功返回 1。
这一小段必须作为一个整体，避免中断插入后把两个不同帧的左右声道拼在一起。

FIFO 可存 256 个双声道帧，硬件每 2048 个 98.304 MHz 时钟消耗一帧，即 48 kHz。
CPU 只负责及时补样本，音频输出节拍由 FPGA 决定。
128 帧约能播放 2.67 ms，因此低水位触发后不能再随意 `delay(10)`。

ΣΔ DAC 将每个 24 bit 样本转换成高速单比特流，经模拟低通与放大得到音频。
`Audio` 本身不合成旋律，也不解码 MP3；样本可以来自计算、数组或外部音频数据。

## 8. Synth 怎样生成声音

`Synth` 是同一个库中更高一层的软件合成器。每个声部保存相位、相位增量、增益、
Attack/Sustain/Release 状态和剩余时长，默认 4 声部，可在菜单选 1–16。

它不是存储一整段录音，而是每个采样点执行类似：

```text
phase += increment
sample = sine_table[phase 的高 8 位]
output += sample × envelope_gain
```

相位是 32 bit，查表是 256 点；增量近似为 `频率 × 2^32 / 48000`。
因此 440 Hz 无须刚好整除 48 kHz，也不受 109 点固定循环的频率限制。
多个声部相加，每次渲染 16 帧，约 0.333 ms；包络在块边界计算并在块内逐帧插值。
查表幅度按声部上限和主音量预缩放，给多声部混音留出幅度空间。

`SYNTH_IRQ` 在 FIFO-low GPIO22 上升沿时自动补帧；`SYNTH_POLL` 则由 `update()`、
`delay()` 和串口等待中的 idle hook 补帧。
`Synth.begin()` 先填静音，确保低水位信号先回落，之后才有新的上升沿。
中断补帧期间会屏蔽 GPIO22，并在重新开启后再检查是否仍低水位，处理丢边沿的情况；
循环有次数上限，并记录 retries/stalls，避免性能不足时一直占用中断。

`tone(pin, frequency)` 在本板包中连接到 Synth 声部 0，pin 参数被忽略，
输出固定走音频 DAC。它不是 AVR Arduino 那种在任意 pin 上输出方波的实现。

## 9. 三个课程重建例子

这些是依据带符号 ELF 重建的行为，不是恢复老师原始 `.c` 文件。

| 示例 | 样本来源与执行方式 |
| --- | --- |
| `CourseBlinky` | 250 ms 后同时翻转 GPIO20/21，两灯同步 |
| `CourseDacSine` | 从精确 109 点表取一个样本，FIFO 有空间才提交并前进下标 |
| `CourseDacSineIRQ` | 同一 109 点表；先填满，GPIO22 低水位回调置标志，主循环醒来补帧 |

两个课程音频例子频率为 `48000/109 ≈ 440.367 Hz`。
IRQ 版的 ISR 只置 `volatile` 标志，不在 ISR 中合成或写音频；这与 Synth 的自动渲染不同。
三份程序已分别通过 12 ms 的 NEORV32 CPU/外设 RTL 仿真；音频 PCM 与 ΣΔ 密度检查无错误，
FIFO 无欠载或丢帧。这个短仿真没有覆盖音频例子数百毫秒的 LED 翻转周期，也不是模拟声音实测。

## 10. 编译、链接和上传

`boards.txt` 定义目标、菜单和存储大小，`platform.txt` 定义 GCC、链接、objcopy 和上传命令。
Arduino 将 `.ino` 作为 C++ 构建，与 core、实际使用的库、NEORV32 驱动和启动代码链接成 ELF。

KR260 当前应用布局：

```text
0xE0000000   128 KiB 程序 BRAM：代码、只读常量、.data 初值镜像
0x80000000    16 KiB DMEM：变量、栈、RAMFUNC
0xFFD00000    音频寄存器
```

ELF 保留段地址、符号和调试信息；BIN 是适合装载的连续程序字节。
GDB 上传根据 ELF 地址写入，CPU 启动还需要单独的 reset/resume 操作；`load` 不等于运行。

UART bootloader 是另一种传输路径。原生 NEORV32 loader 接收 `u` 命令、
`0x4788CAFE` 签名、长度、32 bit 加和取负校验及 BIN，确认成功后用 `e` 跳到应用。
新 KR260 `lab3_kr260_uart` 配置把 loader 放在独立 `0xFFE00000` ROM，应用仍在 `0xE0000000`，
避免上传应用时覆盖正在执行的 loader。2026-11-17已实板验证UART-over-SSH上传、自动启动，
以及GDB的`reset halt → load → resume 0xE0000000 → detach`；两者均通过上传后的独立串口持续运行检查。
当前新增Arduino上传功能尚未发布；本机COM上传、LED外观和模拟电压未实测，详见[验证记录](VALIDATION.md)。
