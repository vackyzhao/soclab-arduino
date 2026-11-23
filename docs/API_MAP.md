# Arduino API、SDK 与硬件映射

本文逐项对应本仓库实际公开的接口，便于从 `.ino` 一直追到 C/C++ 实现、NEORV32 SDK 与寄存器。整体实现原理见 [IMPLEMENTATION.md](IMPLEMENTATION.md)，安装及上传见 [INSTALL.md](INSTALL.md)、[UPLOAD.md](UPLOAD.md)。这是一套自有 Arduino 风格 core；`ARDUINO` 标识不表示已经包含完整 ArduinoCore-API。

以下是当前源码的接口说明。2026-11-17已在实际KR260验证新`lab3_kr260_uart`的115200 UART-over-SSH及新GDB recipe上传和自动启动，两条路径均有独立串口读回确认应用持续执行，见[VALIDATION.md](VALIDATION.md)。当前新增功能尚未发布；本机COM路径、LED外观及新增`analogWrite()`的模拟输出尚未实测。

## 1. 源码入口与层次

文中 `core/`、`SDK/`、`audio/` 分别指以下目录：

| 层 | 文件或目录 | 负责内容 |
| --- | --- | --- |
| Arduino 声明 | [Arduino.h](../hardware/soclab/neorv32/cores/neorv32/Arduino.h) | 时间、GPIO、中断、数学、tone、常量与 SoC 扩展 |
| core | [cores/neorv32/](../hardware/soclab/neorv32/cores/neorv32/) | `main.cpp`、`wiring.cpp`、`trap.c`、`HardwareSerial.*`、`Print.*` |
| SDK | [system/neorv32/sw/lib/](../hardware/soclab/neorv32/system/neorv32/sw/lib/) | 上游 NEORV32 C 驱动、CSR 与 MMIO 类型 |
| 启动和链接 | [sw/common/](../hardware/soclab/neorv32/system/neorv32/sw/common/) | `crt0.S`、`neorv32_arduino.ld` |
| audio | [SocAudio.h](../libraries/SocAudio/src/SocAudio.h)、[SocAudio.cpp](../libraries/SocAudio/src/SocAudio.cpp) | PCM FIFO、软件正弦合成、tone 与 DAC 后端 |
| 变体 | [pins_arduino.h](../hardware/soclab/neorv32/variants/zedboard_soc/pins_arduino.h) | GPIO 位号与别名，variant 名沿用 `zedboard_soc` |
| 板级构建 | [boards.txt](../hardware/soclab/neorv32/boards.txt)、[platform.txt](../hardware/soclab/neorv32/platform.txt) | 菜单、目标、工具链、构建和上传 recipe |
| 上传主机工具 | [uart_upload.py](../tools/uart_upload.py) | 原生 NEORV32 UART loader 的主机端协议 |

板包中的 `hardware/soclab/neorv32/libraries/SocAudio/` 也携带 SocAudio，供板包安装使用。根目录 `libraries/SocAudio/` 是独立 Arduino 库入口；它们是同一实现的打包形式。

```text
.ino / 普通 C++ 文件
        │  Arduino.h、SocAudio.h
        ▼
core：生命周期、时间、GPIO、Print/Serial、中断分发
        │                         │
        │ NEORV32 SDK             │ SocAudio：PCM / 软件 Synth
        ▼                         ▼
CPU CSR、GPIO、UART0          0xFFD00000 音频 MMIO
        │                         │
        ▼                         ▼
目标 RTL / XDC               256 帧 FIFO → 48 kHz → ΣΔ 单比特输出
```

GPIO 数字是 **SoC GPIO 位号**，不是排针脚号。KR260、ZedBoard 的物理连接须分别查看目标 RTL/XDC。KR260 clean Lab3 r6 的 GPIO20/21 对应 UF1/UF2；共同头文件保留了 `LED_D2`、`LED_D3` 的历史名称。

## 2. Arduino 怎样构建和启动

### 2.1 从 sketch 到 ELF

```text
sketch 的 .ino 标签页
  → Arduino 预处理：拼接、必要时加入 Arduino.h、生成函数原型
  → 生成的 C++ 与 sketch 的 .cpp/.c/.S
  → 按 #include 发现库，并加入 core / variant / 库的头文件路径
  → GCC 编译 sketch、core、被选中的库
  → prelink hooks 编译 NEORV32 SDK 驱动和 crt0.S
  → 自定义 linker script 链接 ELF / map
  → objcopy 生成原始 BIN；bin2hex.py 生成仿真用逐字节 HEX
  → 选择 GDB 或原生 UART loader 上传
```

`.ino` 按 Arduino sketch 规则预处理；普通 `.cpp`、`.c`、`.S` 不自动补齐声明，仍须自行包含头文件。复杂 C++ 类型或原型生成不合适时，在源码中写明确的前置声明。

`#include <SocAudio.h>` 不只提供类型声明，也让 Arduino IDE/CLI 发现和编译 SocAudio。`tone()`、虚拟 DAC 的 `analogWrite()` 在 core 中引用弱后端；只调用这些函数而不引入 SocAudio，后端可能没有链接进来。仅有 SDK 的 SPI/TWI/PWM 驱动源码，也不等于 Arduino 的 `SPI`、`Wire` 类或相应 RTL 外设已经提供。

| 构建项 | 当前处理 |
| --- | --- |
| ISA / ABI | `rv32imc_zicsr_zifencei` / `ilp32`；没有 F/D 硬浮点，浮点表达式由软件库处理 |
| 语言 | C `gnu11`；C++ `gnu++17` |
| C++ 运行支持 | 不启用 exceptions、RTTI、线程安全静态局部初始化；默认 heap 大小 0 |
| 优化和配置 | `boards.txt` 菜单控制优化、仿真时间缩放、Synth 声部数、程序空间与上传方式 |
| 链接 | `neorv32_arduino.ld`；sketch/库对象、完整 core archive、SDK 对象及 `crt0.S`，并链接 `m/c/gcc`；未引用段可由 `--gc-sections` 移除 |
| ELF | 保留段地址、符号、调试信息；GDB 按 ELF 地址装载 |
| BIN | `.text`、`.rodata`、`.data` 的装载镜像；不自带 UART loader 的 12 字节头 |
| `.ino.hex` | 每行一个字节，供 RTL `$readmemh` 使用；不是 Intel HEX，也不是原生 loader 文件头 |

应用程序布局：

| 地址 | 内容 | 限制 |
| --- | --- | --- |
| `0xE0000000` | 代码、只读常量、`.data` 初值装载镜像 | KR260 当前 128 KiB 程序 BRAM；ZedBoard 板型为 64 KiB XIP 配置 |
| `0x80000000` | 16 KiB DMEM：变量、`.bss`、RAMFUNC、栈 | `.data.ramfunc` 会占 DMEM；大小显示剩余空间还须留给栈 |
| `0xFFD00000` | 自定义音频外设 | 不是普通 RAM |
| `0xFFE00000` | 新 UART 目标中的独立 boot ROM | 取决于实际载入的 FPGA 配置 |

### 2.2 从复位到 loop

```text
目标 boot ROM / 启动路径
        ↓ 应用入口 0xE0000000
_start（crt0.S）
        ├─ 初始化寄存器和栈
        ├─ .data 初值 → DMEM（包括 .data.ramfunc）
        ├─ 清零 .bss
        └─ 执行 C++ 全局构造函数
        ↓
main（core/main.cpp）
        ├─ 保存构造函数启用的 mie 源
        ├─ neorv32_rte_setup()
        ├─ __arduino_trap_init()：mtvec 指向 DMEM trap 入口
        ├─ 恢复 mie；开启 mstatus.MIE
        └─ setup() 一次
              ↓
          loop() → loop() → loop() …
```

`setup()`、`loop()` 由 sketch 提供，用户不必再定义 `main()`。`yield()` 默认是空的弱函数，`delay()` 等待时调用；`main()` 不在每轮 `loop()` 后自动调用 `yield()` 或 `serialEvent()`。

### 2.3 上传和开始执行是两步

| 上传方式 | 数据与启动动作 | 条件与区别 |
| --- | --- | --- |
| KR260 GDB / OpenOCD | `target extended-remote <server>` → `monitor reset halt` → `load` → `monitor resume 0xE0000000` → `detach` | 装载时停住CPU，再直接执行BRAM应用入口；UART新配置复位会进loader，不能把reset run当作启动应用 |
| ZedBoard GDB / OpenOCD | `target extended-remote <server>` → `load` → `monitor reset run` → `detach` | 复位路径完成XIP初始化，再进入flash应用；不要套用KR260直接resume流程 |
| UART 本地端口 | 原始 BIN 加原生 NEORV32 header；`u` 上传，`e` 启动 | Windows 使用实际 COM 口；需要匹配的 bootloader 已在运行。本地路径不把 DTR/RTS 当作 CPU reset |
| UART 经 KR260 PS/SSH | 同一原生协议，字节流经过 PS `/dev/ttyS0` | 可显式通过独立 PS AXI GPIO 复位 NEORV32，再等待 loader 提示。SSH/Tailscale 不自动生成 Windows COM 口 |
| Upload Using Programmer | 另一个 OpenOCD flash recipe | 原 ZedBoard XIP/flash 路径；不要当成 KR260 BRAM 的通用上传方式 |

上述SSH UART路径已用`BreathingLED`实测：发送5208字节，原生校验通过，`e`跳到`0xE0000000`。新KR260 GDB recipe也已在同一目标实际上传5208字节并detach。两次上传后分别不再次复位或连接GDB，6秒内均收到6个间隔1秒的点。这证明上传和CPU继续执行；500Hz LED波形、亮度和本机COM路径仍需分别实测。

原生 UART 上传序列：`CMD:>` → `u` → `Awaiting neorv32_exe.bin...` → 头和 payload → `OK` 与新提示 → `e` → `Booting from 0xE0000000...`。头为 3 个小端 `uint32_t`：签名 `0x4788CAFE`、payload 字节数、payload 小端 32 位字之和的取负值。字节数不含头，主机工具将 BIN 尾部补零至 4 字节对齐，当前 KR260 上限为补齐后 128 KiB。

运行中的普通应用不会自动解释 `u` 命令。SSH reset 使用 `0xA0030000` 独立 GPIO bit0（1 assert、0 release），沿用 `/run/kr260_rv_mmio.lock` 及已加载 app/PL 时钟检查；不能与持锁的 JTAG 服务同时操作。UART 的 `e` 已负责跳入应用，不再叠加一个 GDB `run`。

## 3. Arduino.h 的全部公开函数

下表保留函数签名；`unsigned long`、`long` 在当前 RV32 ILP32 目标上为 32 位。

### 3.1 生命周期和时间

| API | 实现文件 → SDK/寄存器 | 行为与限制 |
| --- | --- | --- |
| `void setup(void)` | sketch；由 `core/main.cpp` 调用 | 初始化一次 |
| `void loop(void)` | sketch；由 `core/main.cpp` 循环调用 | 返回后立即再次进入 |
| `void yield(void)` | `core/wiring.cpp` 的 weak 空实现 | 可由 sketch 覆盖；`delay()` 调用 |
| `unsigned long millis(void)` | `core/wiring.cpp` → `mcycle/mcycleh` CSR | 按 `F_CPU`、`ARDUINO_TIME_SCALE` 换算；返回值会按 32 位回绕 |
| `unsigned long micros(void)` | 同上 | 换算为 `cycles × 125 × scale / 12288`；不是定时器 ISR 累加 |
| `void delay(unsigned long ms)` | 同上 | 周期忙等；调用 idle hook 和 `yield()`，保留调用者原有的中断开关状态 |
| `void delayMicroseconds(unsigned int us)` | 同上 → `cycles()` | 32 位周期差忙等，不调用 idle hook 或 `yield()`；不是任意长时间的定时器 |
| `uint32_t cycles(void)` | 同上 → `neorv32_cpu_csr_read(CSR_MCYCLE)` | 返回周期低 32 位，约 43.69 秒回绕；不应用软件时间缩放 |
| `void __arduino_set_idle_hook(voidFuncPtr fn)` | 同上 → 单个共享函数指针 | SoC 扩展；供 Synth 轮询补帧等使用，不是可追加多个回调的调度器 |

64 位计数读取采用 `mcycleh → mcycle → mcycleh`，跨低位溢出时重读。这里没有使用 CLINT MTIME 周期中断。`delay()` 不自动 sleep/WFI；CPU 被调试器停住或时钟停止时，周期时间与外部墙上时间的关系取决于硬件计数状态。Simulation 菜单只缩短软件时间等待，音频采样率仍为真实仿真时间的 48 kHz。

### 3.2 数字与模拟输出

| API | 实现文件 → SDK/寄存器 | 行为与限制 |
| --- | --- | --- |
| `void pinMode(uint8_t pin, uint8_t mode)` | `core/wiring.cpp` → 软件 `output_pins` 位图 | GPIO 输入/输出线在 RTL 中固定；没有方向寄存器。`INPUT_PULLUP` 等同 INPUT，没有上拉硬件 |
| `void digitalWrite(uint8_t pin, uint8_t val)` | 同上 → `NEORV32_GPIO->PORT_OUT` | 保存/关闭 IRQ，读改写一个位，再恢复原 IRQ 状态；任意非零 val 为 HIGH |
| `int digitalRead(uint8_t pin)` | 同上 → `PORT_OUT` 或 `PORT_IN` | OUTPUT 读输出锁存器；其他模式读输入。Lab3 仅 GPIO 输入22接音频低水位，其余输入通常接 0 |
| `void digitalToggle(uint8_t pin)` | 同上 → `PORT_OUT ^= 1u << pin` | 同样保护寄存器读改写 |
| `void analogWriteResolution(int bits)` | 同上 → 软件分辨率状态 | 默认 8 bit；限制到 1–24 bit |
| `void analogWrite(uint8_t pin, int value)` | 同上 → GPIO，或 `audio/SocAudio.cpp` 的弱后端 | value 限制到当前无符号范围。GPIO0–23 用半量程阈值输出 LOW/HIGH，**没有 GPIO PWM**；`DAC_LEFT/RIGHT` 经 ΣΔ DAC 输出平均电平 |

数字 pin ≥24：`pinMode`、write、toggle 不操作；`digitalRead` 返回 LOW。`DAC_LEFT=0x80`、`DAC_RIGHT=0x81` 是虚拟 DAC 选择值，不是 GPIO 或排针编号。

DAC analog 后端需要 `#include <SocAudio.h>`。它把选定无符号范围映射为有符号 24 bit PCM（端点为 −8388608 到 +8388607），保留左右通道各自的最新值，再写入一帧。进入静态模式会停止 Synth、复位/使能 DAC、设低水位为 0；首次未设置的另一通道为最小值。FIFO 满时会等待；播放完 FIFO 后硬件保持最后样本。`Audio.begin/end/write` 或 `Synth.begin` 会切换相应的输出所有权。

ΣΔ 引脚仍是高速单比特流，外部低通后才得到平均电压；电压、驱动能力和负载响应取决于板级模拟电路。新增模拟输出尚未做实机测量。

### 3.3 中断

| API | 实现文件 → SDK/CSR | 行为与限制 |
| --- | --- | --- |
| `void attachInterrupt(int irq, voidFuncPtr fn, int mode)` | `core/wiring.cpp` → `neorv32_gpio_irq_disable/setup/enable`、GPIO dispatcher | 参数 irq 实际是 GPIO 位号0–23；先清 pending、配置模式，再启用回调 |
| `void detachInterrupt(int irq)` | 同上 | 禁用该 GPIO 请求、移除回调及 CHANGE 状态 |
| `void interrupts(void)` | 同上 → `mstatus.MIE` bit3 置1 | 全局开中断，不自动配置某个外设 |
| `void noInterrupts(void)` | 同上 → `mstatus.MIE` bit3 清0 | 全局关中断；配对临界段更适合内部 save/restore，以保留进入前状态 |

**Arduino 的触发枚举与 SDK 不同：**

| 意图 | `attachInterrupt()` 的 Arduino 常量 | `neorv32_gpio_irq_setup()` 的 SDK 常量 |
| --- | --- | --- |
| 低电平 | `LOW = 0` | `GPIO_TRIG_LEVEL_LOW = 0` |
| 高电平 | `HIGH = 1` | `GPIO_TRIG_LEVEL_HIGH = 1` |
| 双边沿 | `CHANGE = 2` | 无对应模式；Arduino 根据当前输入交替重设边沿 |
| 下降沿 | `FALLING = 3` | `GPIO_TRIG_EDGE_FALLING = 2` |
| 上升沿 | `RISING = 4` | `GPIO_TRIG_EDGE_RISING = 3` |

不要将 SDK 的裸数值 `3` 原样传给 `attachInterrupt()`：在 SDK 中它是上升沿，在 Arduino API 中它是下降沿。课程 IRQ 重建示例在 Arduino 层使用 `RISING`；它的回调只置标志，主循环再补 FIFO。`Synth` 则通过内部 hook 和 SDK 配置 GPIO22，直接在中断中渲染补帧。

所有 GPIO 共用 FIRQ8（`mcause=0x80000018`），没有嵌套 IRQ。用户回调不会自动放入 DMEM；关键回调可显式加 `RAMFUNC`。Synth IRQ 已使用 GPIO22 时，不要再为同一位安装另一个独立回调。

### 3.4 数学、随机数与 tone

| 全部签名 | 实现 | 行为与限制 |
| --- | --- | --- |
| `long map(long x, long in_min, long in_max, long out_min, long out_max)` | `core/wiring.cpp` | 整数线性换算；乘法用 64 位中间结果；输入跨度为0时返回 out_min，不自动 constrain |
| `void randomSeed(unsigned long seed)` | 同上 | 非零 seed 初始化 xorshift32；0 被忽略 |
| `long random(long max)` | 同上 | `[0,max)`；max≤0返回0 |
| `long random(long min, long max)` | 同上 | `[min,max)`；min≥max返回 min；不是 SDK TRNG、不会自动用硬件熵播种 |
| `void tone(uint8_t pin, unsigned int frequency, unsigned long duration)` | `core/wiring.cpp` → `__arduino_tone` → `audio/SocAudio.cpp` | SocAudio 声部0，pin 忽略；duration=0持续到 noTone；需要链接 SocAudio |
| `tone(uint8_t pin, unsigned int frequency)` | `Arduino.h` 内联 | 等同三参数版 duration=0 |
| `void noTone(uint8_t pin)` | `core/wiring.cpp` → `__arduino_no_tone` | 关闭声部0；pin 忽略；输出固定走音频 DAC，不是任意 pin 方波 |

## 4. 常量、类型和宏

| 名称 | 数值/处理 | 限制或作用 |
| --- | --- | --- |
| `ARDUINO`、`ARDUINO_ARCH_NEORV32`、`ARDUINO_SOCLAB_NEORV32` | `10819`、`1`、`1` | 条件编译标识 |
| `F_CPU` | `98304000UL` | NEORV32 应用时钟98.304 MHz，不是 PS AXI 的100 MHz |
| `ARDUINO_SIM`、`ARDUINO_TIME_SCALE` | 默认0、1 | 构建菜单可覆盖；影响软件计时 |
| `ARDUINO_SIM_BAUD` | `1228800UL` | SIM 模式 Serial 强制使用此波特率 |
| `RAMFUNC_SECTION`、`RAMFUNC` | `.data.ramfunc`；section + noinline attribute | crt0 随 `.data` 复制到 DMEM，减少 XIP 关键路径等待；占用 DMEM |
| `LOW`、`HIGH` | 0、1 | 电平及 level IRQ 模式 |
| `INPUT`、`OUTPUT`、`INPUT_PULLUP` | 0、1、2 | 只记录软件读语义，不修改物理方向/上拉 |
| `CHANGE`、`FALLING`、`RISING` | 2、3、4 | Arduino 模式，勿与 SDK 数字混用 |
| `LSBFIRST`、`MSBFIRST` | 0、1 | 仅常量；不代表 shift/SPI 类已经实现 |
| `DEC`、`HEX`、`OCT`、`BIN` | 10、16、8、2 | Print 的进制参数 |
| `NUM_DIGITAL_PINS` | 24 | GPIO0–23 |
| `LED_BUILTIN`、`LED_D2`、`LED_D3` | 20、20、21 | 物理灯由目标 RTL/XDC 决定 |
| `PIN_AUDIO_LOW` | 22 | 音频 FIFO 低水位输入 GPIO22 |
| `AUDIO_LOW_PIN` | 22 | 仅 `pins_arduino.h` 的别名 |
| `NUM_ANALOG_INPUTS` | 0 | 仅 `pins_arduino.h`；当前没有 Arduino ADC API |
| `DAC_LEFT`、`DAC_RIGHT` | `0x80`、`0x81` | analogWrite 的虚拟音频 DAC 选择 |
| `NOT_AN_INTERRUPT` | −1 | 非 GPIO interrupt 标记 |
| `digitalPinToInterrupt(p)` | 有效0–23返回 p，否则−1 | GPIO位号没有额外 remap |
| `PI`、`HALF_PI`、`TWO_PI`、`DEG_TO_RAD`、`RAD_TO_DEG`、`EULER` | 数学常数 | 普通数值常量 |
| `boolean`、`byte`、`word` | `bool`、`uint8_t`、`uint16_t` | 类型别名 |
| `voidFuncPtr` | `void (*)(void)` | 无参数、无返回值回调 |
| `bit(b)`、`_BV(b)` | `1UL << b` | 软件位运算，位号须适合操作数宽度 |
| `bitRead(v,b)`、`bitSet(v,b)`、`bitClear(v,b)`、`bitToggle(v,b)`、`bitWrite(v,b,x)` | 测试、设置、清除、翻转、按值写位 | 宏本身不为共享 MMIO 提供 IRQ 临界段 |
| `lowByte(w)`、`highByte(w)` | 取低字节、右移8后取字节 | 不自动读外设 |
| `radians(deg)`、`degrees(rad)`、`sq(x)` | 软件表达式宏 | 宏参数可能重复求值，避免带副作用表达式 |
| `clockCyclesPerMicrosecond()` | `F_CPU / 1000000.0` =98.304 | 浮点表达式 |
| `F(s)` | 原样返回 s | 统一地址空间，不是 AVR 的独立 flash-string 类型或 PROGMEM 机制 |
| `min(T,U)`、`max(T,U)`、`constrain(T,L,H)` | C++ 内联模板；C 中为宏 | 纯软件比较，不映射 SDK 外设 |

标准 C 头中的 `abs`、`memcpy`、`sin` 等由编译器/newlib/libm 提供；它们不是额外的 Arduino 包装函数。

## 5. Serial 与 Print 的全部公开接口

### 5.1 HardwareSerial

`HardwareSerial final : public Print`，当前全局实例只有 `Serial`，实现为 [HardwareSerial.cpp](../hardware/soclab/neorv32/cores/neorv32/HardwareSerial.cpp)。

| 全部公开签名 | SDK/寄存器映射与语义 |
| --- | --- |
| `constexpr HardwareSerial()` | C++ 对象构造；初始 baud 为0 |
| `void begin(unsigned long baud, uint8_t config=SERIAL_8N1)` | `neorv32_uart0_setup()`，启用 UART0 RX-not-empty 中断；baud=0退回19200；SIM强制1228800。config 当前忽略，只支持8N1 |
| `void end()` | 等发送完、移除 RX handler/poll hook、禁用 UART，baud 状态清0 |
| `int available()` | 软件 RX 环形缓冲中可读字节数 |
| `int peek()` | 查看下一个字节但不移除；空时−1 |
| `int read()` | 移除一个字节；空时−1 |
| `int availableForWrite()` | 检查 UART TX_FULL；返回0或1，不是整段可用 FIFO 容量；未 begin 返回0 |
| `void flush()` | 等 UART TX_BUSY 清除，即最后一个字符离开发送器；不清 RX |
| `size_t write(uint8_t c)` | 等 TX 空间，写 UART0 DATA 低8位；等待及发送后调用 idle hook；未 begin 返回0 |
| `size_t write(const uint8_t *buf, size_t len)` | 逐字节走同一 TX 路径，返回已发送字节数 |
| `using Print::write` | 也支持 `const char *` 与带长度的 char buffer |
| `unsigned long baud() const` | 返回当前设置的 baud 软件状态 |
| `uint32_t overruns() const` | 软件 ring 丢字节计数，加硬件 RX overrun sticky 事件记录；不是精确硬件丢字节总数 |
| `explicit operator bool() const` | baud 状态非0为 true，不表示已连接到串口终端 |

常量 `SERIAL_8N1=0`；`SERIAL_RX_BUFFER_SIZE` 默认64且要求2的幂。环形缓冲留一个空位区别满/空，默认有效容量63字节。UART RX 硬件 FIFO 当前一字符深；ISR 和音频补帧中的 RX poll 都将它搬入 ring。长时间不读取仍会丢字节。

UART0 的 RX 使用 FIRQ2（`mcause=0x80000012`）。TX/RX搬运关键代码在 DMEM。KR260 路径为 `NEORV32 UART0 ↔ PL CDC ↔ PS AXI16550 ↔ /dev/ttyS0`；OpenOCD3333是另一条调试连接。

### 5.2 Print

[Print.h](../hardware/soclab/neorv32/cores/neorv32/Print.h) / [Print.cpp](../hardware/soclab/neorv32/cores/neorv32/Print.cpp) 是文本格式化基类；最终通过虚函数 `write()` 输出，不直接访问某个 UART。完整重载如下：

```cpp
constexpr Print();
virtual size_t write(uint8_t c) = 0;
virtual size_t write(const uint8_t *buf, size_t len);
size_t write(const char *s);
size_t write(const char *buf, size_t len);
virtual int availableForWrite();  // 基类默认0
virtual void flush();            // 基类默认空

size_t print(const char *s);
size_t print(char c);
size_t print(signed char n, int base=10);
size_t print(unsigned char n, int base=10);
size_t print(short n, int base=10);
size_t print(unsigned short n, int base=10);
size_t print(int n, int base=10);
size_t print(unsigned int n, int base=10);
size_t print(long n, int base=10);
size_t print(unsigned long n, int base=10);
size_t print(long long n, int base=10);
size_t print(unsigned long long n, int base=10);
size_t print(double x, int digits=2);

size_t println(void);
template<typename T> size_t println(T v);
template<typename T> size_t println(T v, int fmt);
```

整数格式化支持base2–36，范围外退回10；负号仅用于十进制，其他进制按相应无符号位模式输出。小整数重载转发到 long/unsigned long；bool 提升为 int，输出1/0。浮点小数位限制0–9，处理 nan/inf/ovf；没有硬浮点。`println` 追加 `\r\n`。字符串长度读取走 DMEM helper，再调用字节输出。

这里没有 `print(bool)` 或通用指针输出重载；非 char 指针不能靠隐式转 bool 悄悄输出“1”。`Print` 也没有继承 `Stream`，因此 `parseInt()`、`readBytes()`、`readString()`、`find()`、`setTimeout()` 不存在。

## 6. Audio：全部 PCM 接口与寄存器

全局实例 `Audio`，类型 `SocAudioClass`；全部实现在 `audio/SocAudio.cpp`。

| 全部公开签名 | 操作 | 限制 |
| --- | --- | --- |
| `constexpr SocAudioClass()` | 对象构造 | 不代表已启动硬件 |
| `void begin(uint16_t lowWater=0)` | CTRL写3再写2，复位并使能 FIFO/DAC，设置阈值 | 停止运行中的 Synth，退出 analog 静态模式 |
| `void end()` | 停 Synth、复位 FIFO、CTRL清0 | ΣΔ输出保持低，对应负满量程，模拟通路可能有瞬态；需要静音保持可用 `Synth.end()` |
| `void setLowWater(uint16_t lowWater)` | 写 FIFO_LOW | 普通模式取低9位；Synth IRQ运行时限制16–224，避免无法产生新上升沿 |
| `int level()` | 读 FIFO_LEVEL 低9位 | 帧数0–256 |
| `int availableForWrite()` | 256减 level | 单位是双声道帧，不是字节 |
| `bool full()`、`bool empty()`、`bool low()` | 读 STAT0 的对应状态位 | low条件为 level < lowWater；lowWater=0永不low |
| `size_t write(int32_t left, int32_t right)` | 饱和到 signed24，写 L，再写 R加bit31 commit | 临界段内检查FULL并成对提交；满返回0、成功1，不阻塞。首次从 analog 静态模式切回时清除静态队列 |
| `size_t write(int32_t mono)` | 内联转发 write(mono,mono) | 同时写左右声道 |
| `size_t write16(int16_t left, int16_t right)` | 各乘256再转发 | 将16位PCM移到24位幅度范围；没有单参数 write16 重载 |

全部公开寄存器与数值宏：

| 宏 | 地址/值 | 内容 |
| --- | --- | --- |
| `SOC_AUDIO_BASE` | `0xFFD00000` | 音频基址 |
| `SOC_AUDIO_CTRL0` | base+`0x00`，volatile32 | `SOC_AUDIO_CTRL_RESET=1<<0`、`SOC_AUDIO_CTRL_ENABLE=1<<1` |
| `SOC_AUDIO_STAT0` | base+`0x04`，volatile32 | `SOC_AUDIO_STAT_LOW=1<<0`、`SOC_AUDIO_STAT_EMPTY=1<<1`、`SOC_AUDIO_STAT_FULL=1<<2` |
| `SOC_AUDIO_FIFO_LOW` | base+`0x08`，volatile32 | 低水位阈值 |
| `SOC_AUDIO_FIFO_LEVEL` | base+`0x0C`，volatile32 | 当前帧数 |
| `SOC_AUDIO_L` | base+`0x10`，volatile32 | 左声道低24位暂存 |
| `SOC_AUDIO_R` | base+`0x14`，volatile32 | 右声道低24位；`SOC_AUDIO_COMMIT=1<<31`提交一帧 |
| `SOC_AUDIO_FIFO_DEPTH` | 256 | 双声道帧容量 |
| `SOC_AUDIO_RATE` | 48000 | 硬件每2048个98.304MHz周期取一帧 |
| `SOC_AUDIO_MAX`、`SOC_AUDIO_MIN` | 8388607、−8388608 | signed24样本端点 |

FIFO 空后 DAC 保持最后一个样本。`Audio` 只接收 PCM，不负责 wav/MP3 文件解析、重采样或旋律生成；自定义样本可以来自代码计算、数组或文件解码后的数据。

## 7. Synth：全部合成与统计接口

全局实例 `Synth`，类型 `SynthClass`。它是 CPU 上的软件合成器：256点正弦表、32位相位累加、逐声部增益和包络，按16帧一块渲染，再经 Audio FIFO 输出。

| 全部公开签名 | 行为与实现限制 |
| --- | --- |
| `constexpr SynthClass()` | 对象构造，不自动开始播放 |
| `void begin(uint8_t mode=SYNTH_IRQ, uint16_t lowWater=128)` | 停旧 Synth，初始化状态/查表，必要时启用 Audio，预填静音；无效mode按IRQ；阈值限制16–224 |
| `void end()` | 声部在一个块内淡出到0并停止补帧；DAC保持使能，队列播放完后保持0 |
| `bool running() const` | Synth补帧是否运行 |
| `void noteOn(uint8_t voice, uint8_t note, uint8_t velocity=127)` | MIDI音高查相位增量，note按低7位；A4为名义440Hz；velocity限制到127，0等同noteOff |
| `void noteOnHz(uint8_t voice, float hz, uint8_t velocity=127, uint32_t duration_ms=0)` | 直接指定频率与时长；duration0持续；时长按ARDUINO_TIME_SCALE缩放 |
| `void noteOff(uint8_t voice)` | 进入release包络 |
| `void allOff()` | 所有声部进入release |
| `void setFrequency(uint8_t voice, float hz)` | 改音高，不重新触发包络 |
| `void setEnvelope(uint16_t attack_ms, uint16_t release_ms)` | 默认5/20ms；真实音频毫秒，不跟仿真软件时间缩放；按块向下量化，0最少一个块 |
| `void setVolume(uint8_t volume)` | 主音量0–255，重建预缩放正弦表 |
| `bool isPlaying(uint8_t voice) const` | 对应声部是否仍在播放/包络阶段；无效voice返回false |
| `uint32_t frequencyMilliHz(uint8_t voice) const` | 根据相位增量得到量化后的精确数字频率；无效voice返回0 |
| `void update()` | POLL模式主动补帧；IRQ模式主要由中断补帧 |
| `SynthStats stats() const` | 保存IRQ状态后复制统计并计算elapsed_cycles |
| `void resetStats()` | 清统计并重置周期起点 |
| `void printStats(Print &out, const char *label) const` | 经Print输出帧数、cycles/frame、IRQ、负载、重试/停顿等一行统计 |
| `static uint32_t noteIncrement(uint8_t note)` | MIDI note低7位查相位增量 |
| `static uint32_t hzToIncrement(float hz)` | 约为round(hz×2³²/48000)；非正或NaN返回0，超过24kHz限制到24kHz |

无效voice索引的写操作被忽略。`SYNTH_VOICES` 默认4，可编译配置1–16；`SYNTH_BLOCK=16`、`SYNTH_IRQ=0`、`SYNTH_POLL=1`。混音查表按声部上限和主音量预缩放，所以配置的声部上限会影响单声部幅度。

IRQ模式占用GPIO22的FIFO-low上升沿；补帧期间屏蔽该位，结束后重启并检查是否仍low，有限次重试以处理错过边沿的情况。POLL模式依赖显式`update()`或由`delay()`、Serial输出路径调用的idle hook；长计算循环和`delayMicroseconds()`不会自动补帧。

`SynthStats` 的全部公开字段：

| 字段 | 类型 | 统计含义 |
| --- | --- | --- |
| `frames` | `uint32_t` | 写入FIFO的帧数 |
| `render_cycles` | `uint64_t` | 渲染和写帧的CPU周期 |
| `irqs` | `uint32_t` | FIFO-low处理次数 |
| `irq_cycles` | `uint64_t` | ISR中的CPU周期总量 |
| `irq_cycles_max` | `uint32_t` | 单次ISR最大周期 |
| `irq_frames_min`、`irq_frames_max` | `uint32_t` | 每次ISR补帧数范围 |
| `elapsed_cycles` | `uint64_t` | 自resetStats起经过的CPU周期 |
| `voice_blocks` | `uint32_t` | 所有声部渲染块数之和 |
| `mul_blocks` | `uint32_t` | 需要非单位增益乘法的声部块数 |
| `refill_retries` | `uint32_t` | 一轮补帧后仍低水位而继续的次数 |
| `stalls` | `uint32_t` | 有限重试结束后仍低水位的次数 |

这些统计是CPU执行证据，不是模拟输出电压、扬声器音量或失真的测量值。

## 8. 内部 hook、中断路径与 MMIO

这些接口供core和库协作，见 [arduino_internal.h](../hardware/soclab/neorv32/cores/neorv32/arduino_internal.h)，不是稳定的通用Arduino sketch接口。

| 内部接口/符号 | 实现文件 | 下层映射与约束 |
| --- | --- | --- |
| `__arduino_handler_t` | `arduino_internal.h` | `void (*)(void)` |
| `__arduino_irq_install(uint32_t code, fn)` | `core/trap.c` | DMEM表的code低5位；安装/移除时同步设置/清除CSR_MIE相应位，临界段保存原MIE |
| `__arduino_trap_init()` | `core/trap.c` | CSR_MTVEC指向DMEM的trap入口 |
| `__arduino_trap_entry()` | `core/trap.c` | 裸汇编入口：80字节、16字节对齐栈帧，保存caller-saved寄存器和mepc，返回时mret |
| `__arduino_trap_dispatch(mcause, frame)` | `core/trap.c` | IRQ按mcause低5位查表；异常/未处理IRQ交给RTE debug路径 |
| `__arduino_gpio_handler(int pin, fn)` | `core/wiring.cpp` | 安装GPIO回调，不处理Arduino mode；调用者用SDK配置trigger，例如Synth的GPIO22 |
| `__arduino_idle_hook` | `core/wiring.cpp` | 单一共享hook；delay和Serial输出可调用，Synth POLL使用它 |
| `__arduino_rx_poll_hook` | 同上，Serial.begin设置 | DMEM的UART RX搬运函数；供关IRQ的长音频补帧每块调用，避免与RX ISR并发 |
| `__arduino_irq_save()` | `arduino_internal.h`内联 | `csrrci mstatus,8`：返回原MIE位并关闭IRQ |
| `__arduino_irq_restore(uint32_t s)` | 同上 | 仅在原状态启用时重新启用，不无条件开IRQ |
| `__arduino_tone(pin,frequency,duration_ms)` | `audio/SocAudio.cpp` | core弱引用，映射Synth声部0 |
| `__arduino_no_tone(pin)` | 同上 | core弱引用，关闭声部0 |
| `__arduino_analog_write(pin,value,bits)` | 同上 | core弱引用，映射静态PCM平均电平 |
| `__cxa_pure_virtual()`、`__dso_handle`、`operator delete(void*)`及带size重载 | `core/main.cpp` | 最小C++链接支持；纯虚调用停在循环，delete为空实现，不构成完整动态内存管理 |

```text
GPIO22 FIFO-low边沿                  UART0 RX-not-empty
       ↓ GPIO IRQ_ENABLE                  ↓ UART CTRL IRQ enable
FIRQ8 / mie bit24                    FIRQ2 / mie bit18
       └────────────────┬──────────────────┘
                    mstatus.MIE=1
                         ↓
           mtvec → __arduino_trap_entry（DMEM）
                         ↓ 保存现场，IRQ期间MIE=0
               __arduino_trap_dispatch
                 ├─ GPIO gpio_dispatch
                 │     ├─ 读IRQ_PENDING、确认pending
                 │     ├─ CHANGE重设下一个边沿
                 │     └─ 调用pin_handler[pin]
                 │             └─ sketch回调 / synth_isr
                 └─ UART serial_rx_drain → RX ring
                         ↓
                    恢复现场 → mret
```

GPIO pending通过`IRQ_PENDING = ~handled_bits`清除已处理位。用户回调自身及其调用链是否在DMEM，取决于编译section；dispatcher在DMEM并不自动搬移所有下游函数。未处理trap会调用上游RTE debug，致命访问错误可能停止执行，其他异常按RTE处理后调整返回PC。

| 外设/CPU | 基址或CSR | core实际使用的成员/位 |
| --- | --- | --- |
| GPIO | `0xFFFC0000` | `PORT_IN +0x00`、`PORT_OUT +0x04`、`IRQ_TYPE +0x10`、`IRQ_POLARITY +0x14`、`IRQ_ENABLE +0x18`、`IRQ_PENDING +0x1C` |
| UART0 | `0xFFF50000` | `CTRL +0x00`、`DATA +0x04`；RX_NEMPTY bit16、TX_FULL bit21、RX-not-empty IRQ enable bit22、RX_OVER bit30、TX_BUSY bit31 |
| CPU中断 | `mstatus`、`mie`、`mtvec`、`mcause`、`mepc` | 全局MIE bit3；UART0_RX FIRQ2/code18；GPIO FIRQ8/code24 |
| CPU计时 | `mcycle`、`mcycleh` | 时间、延迟、Synth统计 |
| 自定义音频 | `0xFFD00000` | 上一节的6个寄存器；不经SDK标准音频驱动 |

GPIO SDK对应`neorv32_gpio.c/.h`，UART对应`neorv32_uart.c/.h`，CSR对应`neorv32_cpu.h`，异常诊断对应`neorv32_rte.c`。标准外设驱动的存在仍需与当前目标RTL的SYSINFO/实例化配置对应。

## 9. 当前没有提供的常见Arduino部分

| 名称/能力 | 当前状态与迁移方式 |
| --- | --- |
| `String` / `WString`、`Stream` | 未提供；使用定长char缓冲及现有Print/Serial字节接口 |
| `Serial.parseInt/readBytes/readString/find/setTimeout` | 未提供；可在应用中基于available/read实现明确的解析状态机 |
| `Wire`、Arduino `SPI` 类、`EEPROM`、`Servo`、`SoftwareSerial` | 未提供；SDK中相近驱动不等于Arduino wrapper，也不保证当前RTL存在该外设 |
| `analogRead`、`analogReference`、`analogReadResolution` | 未提供；当前模拟输入数为0 |
| 任意GPIO PWM或任意pin `tone` | 未提供；普通analogWrite是数字阈值，tone固定走音频DAC |
| `pulseIn`、`pulseInLong`、`shiftIn`、`shiftOut` | 未提供；LSBFIRST/MSBFIRST只保留常量 |
| `serialEvent`、标准Arduino `init()`入口 | 未提供；本core的main直接管理启动并调用setup/loop |
| USB/HID、网络Client/Server/IPAddress | 未提供；PS Linux网络不是NEORV32 Arduino网络栈 |
| 完整动态内存、exceptions、RTTI、RTOS线程 | 不属于当前配置；默认heap为0，用静态对象/缓冲并控制DMEM与栈用量 |

移植第三方Arduino库时，先核对它实际调用的类和函数，再核对目标RTL外设及内存预算。可编译的头文件、可链接的SDK函数、RTL仿真通过、实际FPGA运行与模拟音频测量，是不同层次的验证结果。
