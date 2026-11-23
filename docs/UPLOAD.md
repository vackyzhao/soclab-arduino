# 上传、CPU 启动及硬件适配

## 选择上传方式

KR260 的 **工具 → Upload method** 提供三种方式：

| 菜单 | FQBN 选项 | 电脑到 NEORV32 的路径 |
| --- | --- | --- |
| GDB / OpenOCD | `uploadmethod=gdb` | GDB → 本机或远端 OpenOCD → CPU JTAG → ELF 段 |
| UART 115200 through KR260 SSH | `uploadmethod=uartssh` | 本机 Python/SSH → KR260 PS `/dev/ttyS0` → NEORV32 原生 loader |
| UART 115200 through local serial port | `uploadmethod=uartlocal` | 本机 Python/pyserial → 所选串口 → NEORV32 原生 loader |

这三个选项使用同一个已编译 sketch；UART 发送 BIN，GDB 按 ELF 的段地址装载。**OpenOCD target** 的 Local/Remote 只影响 GDB，不决定 UART SSH 的地址。

UART 菜单及以下配置已随 **0.3.0** 板包提供。已有 Boards Manager 安装直接更新到 0.3.0；手动 core 使用完整离线安装器更新实际 sketchbook，保留本地连接设置。仅下载源码或安装 SocAudio 库 ZIP 不会更新上传脚本，见[安装说明](INSTALL.md)。2026-11-17已在实际KR260加载`lab3_kr260_uart`，用115200 UART-over-SSH上传5208字节的`BreathingLED`并自动启动；同一新目标上的下述GDB recipe也已实板上传通过。两条路径在上传后分别通过独立6秒串口观察确认持续运行，见[VALIDATION.md](VALIDATION.md)。呼吸灯外观已有用户确认；本机COM上传和模拟输出电压尚未实测。

## KR260 的 GDB Upload 命令

```sh
riscv-none-elf-gdb -batch -nx \
  -ex "set confirm off" \
  -ex "target extended-remote 127.0.0.1:3333" \
  -ex "monitor reset halt" \
  -ex "load" \
  -ex "monitor resume 0xE0000000" \
  -ex "detach" \
  YourSketch.ino.elf
```

- `target extended-remote` 连接已经运行的 OpenOCD。选择地址不会自动启动远端服务。
- `monitor reset halt` 复位并停住 CPU，使应用不在装载期间继续执行旧代码。
- `load` 按 ELF 段地址写入程序；内存如何写入由目标 OpenOCD 配置决定。
- `monitor resume 0xE0000000` 从应用 `_start` 入口继续执行，启动汇编再初始化 `.data`、`.bss`、栈和 C++ 运行环境。KR260 的应用在 BRAM，不需要在此初始化 XIP。
- `detach` 断开调试连接。不要用一个额外的 GDB 连接测试“是否已运行”，因为连接调试器可能再次暂停 CPU。

旧 r6 的 `load → reset run` 已经实际运行通过；新增 UART 配置把复位入口改为独立 boot ROM，所以继续使用 `reset run` 会进入 loader 等待命令，而不是自动执行刚由 GDB 装入的应用。KR260 现在在 **boards.txt** 覆盖为上面的 `reset halt → load → resume 0xE0000000`，兼容直接 BRAM 应用入口。

2026-11-17已用`uploadmethod=gdb,openocd=remote`在新目标实际执行该流程：5208字节ELF装载、detach后退出码0，随后不重新连接调试器的串口读取连续6秒收到6个间隔1秒的`.`。日志中的libgcc源文件路径缺失警告仅表示本机没有编译器内部的源码文件，并非装载或执行失败。

ZedBoard 仍沿用 **platform.txt** 中的 `load → monitor reset run → detach`：其 boot ROM 必须先完成 XIP 初始化，再跳入 flash 应用。不要把 KR260 的直接 BRAM resume 流程套到刚复位的 ZedBoard XIP 目标。

## 自定义 GDB / SSH 地址

在 **工具 → OpenOCD target** 选择 Local 或 Remote。远端地址在当前平台目录旁的 `boards.local.txt` 中设置，例如：

```ini
zedboard.menu.openocd.remote=Remote lab PC
zedboard.menu.openocd.remote.tools.gdb.server=192.0.2.1:3333
kr260.menu.openocd.remote=Remote KR260
kr260.menu.openocd.remote.tools.gdb.server=192.0.2.2:3333

# UART through SSH 独立使用以下属性，不读取上面的 GDB server 地址。
kr260.tools.uartssh.ssh=ubuntu@192.0.2.2
kr260.tools.uartssh.device=/dev/ttyS0
kr260.tools.uartssh.resetaddress=0xA0030000
```

以上是示例地址，请换成自己的局域网或 Tailscale IP；SSH 也可以填已配置的 `~/.ssh/config` 主机别名。修改后重新打开 IDE。Boards Manager 更新到新版本时检查新版本目录中的本地设置；手动离线安装器会保留原目录的本地配置。平台目录为手动安装时的 `<sketchbook>/hardware/soclab/neorv32`，或 Boards Manager 的 `<Arduino data>/packages/soclab/hardware/neorv32/<版本>`。`boards.local.txt` 属于本机配置，不放用户名密码或提交到仓库。

先配置上面的远端地址，然后使用 Arduino CLI：

```sh
arduino-cli compile --fqbn soclab:neorv32:kr260:build=board,openocd=remote,uploadmethod=gdb --output-dir build ./Blink
arduino-cli upload --fqbn soclab:neorv32:kr260:build=board,openocd=remote,uploadmethod=gdb --input-dir build ./Blink
```

`compile` 生成程序，`upload` 只上传已经生成的文件。IDE 与 CLI 共用 `boards.local.txt` 配置。

## UART 115200：一次配置，之后点 Arduino 上传

### KR260 PS/SSH 路径

1. 在 KR260 配置匹配的 `lab3_kr260_uart` FPGA app/设备树。此配置包含 NEORV32 原生 loader（115200、8N1、无流控）以及独立 CPU reset GPIO。旧 r6 只有 UART 通路，不能仅靠换 Arduino 菜单获得 bootloader。
2. 在 `boards.local.txt` 填好 SSH 目标、PS 侧串口及 reset 地址。0.3.0 的对应主机工具依赖或完整离线包提供 Python 3.12.16，PC 仍需要操作系统的 OpenSSH `ssh`；PS 需要其自身的 Python 3。SSH 桥使用 Python 标准库，不需要额外安装 pyserial。
3. 完成正常 SSH 的密钥/主机配置，并在 PS 按硬件运行环境配置好既有的非交互 sudo 权限。上传工具使用 `BatchMode=yes` 和 `sudo -n`，不在 IDE 内询问 SSH/sudo 密码，也不修改 `/dev/mem` 的权限。
4. 切到 UART 模式前停止 `lab3-openocd`、`lab3-jtag`，并关闭使用 `/dev/ttyS0` 的串口终端。JTAG 桥常驻持有 `/run/kr260_rv_mmio.lock`，与 reset/PL 装卸共用同一把锁；上传工具发现锁忙会报错，不会自行终止服务。
5. IDE 选 **KR260 → Build: Board (real time) → Upload method: UART 115200 through KR260 SSH**，点击普通 **上传**。此路径不需要选择本机 COM 口。

例如切换前可在已配置的 PS 上停止调试服务：

```sh
ssh ubuntu@192.0.2.2 "sudo systemctl stop lab3-openocd.service lab3-jtag.service"
```

reset helper 先检查 app 名 `lab3_kr260_uart`、FPGA operating 状态、PL 时钟、设备树 `cpu-reset@a0030000` 的 compatible 和 reg，然后才访问独立 `0xA0030000` AXI GPIO：bit0写1断言 CPU reset，清串口残留后写0释放。原 JTAG GPIO 地址不变，PS UART 不随这个 CPU reset 一起复位。若载入旧 bit 或 DTS 不匹配，工具停止并给出错误。

UART 上传过程自动完成：

```text
CPU reset → 等待 CMD:> → u → 发送原生头与 BIN
          → 收到 OK → e → Booting from 0xE0000000...
```

应用开始执行由 loader 的 `e` 命令完成，**不再需要手动 GDB run**。应用仍在易失 BRAM，重新加载 PL/断电后需要重新上传。切回 GDB 时选择 `uploadmethod=gdb`，再按 PS 环境启动 `lab3-jtag` 和 `lab3-openocd`；UART uploader 不会替用户切换这两项服务。

CLI 对应选项：

```sh
arduino-cli compile --fqbn soclab:neorv32:kr260:build=board,uploadmethod=uartssh --output-dir build ./Blink
arduino-cli upload --fqbn soclab:neorv32:kr260:build=board,uploadmethod=uartssh --input-dir build ./Blink
```

### 本机串口路径

IDE 选择 **Upload method: UART 115200 through local serial port**，再在 **端口** 中选择实际连接 NEORV32 UART 的 COM/tty。0.3.0 的对应主机 Python 工具依赖和完整离线包已带 pyserial 3.5，无需另装 pip 包。只有从源码自选 Python 安装时，才需向 `platform.local.txt` 的 `soclab.python` 所指解释器安装 pyserial，例如 `/完整路径/python3 -m pip install pyserial==3.5`；不要假定系统 `python` 就是 IDE 使用的解释器。之后日常只需点上传。

这条路径要求 loader 已经运行并等待命令；脚本不把 USB 串口的 DTR/RTS 当作 CPU reset。正在运行的普通 sketch 不会自动响应 `u`。KR260 PS 控制台 USB 口也不自动等于 PL 中 NEORV32 的 UART，必须有正确接线或透明串口桥。

```sh
arduino-cli upload --fqbn soclab:neorv32:kr260:build=board,uploadmethod=uartlocal --port COM6 --input-dir build ./Blink
```

上传日志会核对 loader 的 `OK`、新提示和 `Booting from 0xE0000000...`。`ERR_EXE`、`ERR_SIZE`、`ERR_CHKS` 或异常不会被当作成功；原生 loader 出错后可能停在等待状态，需要复位后重试。协议细节见 [API_MAP.md](API_MAP.md)。

## 硬件布局

| 目标 | CPU / DMEM | 程序地址 | 程序存储 | 音频 |
|---|---|---|---|---|
| 课程 ZedBoard Lab3 | NEORV32 v1.11，约 98.304 MHz / 16 KiB | `0xE0000000` | XIP SPI flash，平台限制 64 KiB | 匹配课程扩展板与约束 |
| KR260 clean Lab3 r6 | 同一软件外设布局 / 16 KiB | `0xE0000000` | 可写 BRAM，128 KiB；重新加载 PL 或断电后丢失 | PMOD1/J2 物理 1=左、3=右；9/10=GND |
| KR260 `lab3_kr260_uart`（UART-over-SSH及新GDB已实测） | Lab3 外设与独立 loader / 16 KiB | 应用 `0xE0000000`；loader `0xFFE00000` | 应用 BRAM 128 KiB；新增 PS reset GPIO `0xA0030000` | 沿用 clean Lab3 的 J2 音频连接 |

音频 MMIO 基址为 `0xFFD00000`，24 bit 有符号左右声道，FIFO 256 帧，采样率名义 48 kHz。`LED_BUILTIN` 对应 SoC GPIO20；KR260 r6 对应 UF1，GPIO21 对应 UF2。这是特定 bitstream 的映射，不是任意 KR260 工程的通用引脚表。

KR260 r6 将 PMOD J2 的上述两根脚设为输出，J21 不作音频输出。输出为 3.3 V 逻辑的 ΣΔ 位流，需匹配的模拟滤波及放大路径，不能把一个未滤波的逻辑引脚当作已验证的耳机输出。

## OpenOCD 和串口

ZedBoard 的 `tools/openocd_neorv32_jtaghs2.cfg` 针对课程扩展板的 CPU JTAG 与 XIP flash。它不适用于把 FPGA 配置 JTAG 自动当成 CPU JTAG。

KR260 clean Lab3 使用 PS 到 AXI GPIO 的 CPU JTAG 桥，并在 PS 上运行 OpenOCD；PS 侧桥接程序、设备树和 bitstream 属于硬件工程，不随此 Arduino core 安装。KR260 不能使用 ZedBoard 的 **Upload Using Programmer** SPI flash 流程；普通 **Upload** 根据菜单选择 GDB 或 UART。

`Serial` 是 NEORV32 UART。KR260 的 UART 经 PS 侧 AXI16550 成为 Linux ttyS 设备；GDB/Tailscale 地址不等于一个 Windows COM 口。需要在 PS 上读写相应串口，或另行配置串口转发。

## 没闪灯／没声音时

1. 确认使用 Board 实时时钟模式，且已配置对应 FPGA bitstream。
2. 按方式检查启动动作：KR260 GDB为`monitor resume 0xE0000000`，ZedBoard GDB为`monitor reset run`，UART为`Booting from 0xE0000000...`；不能仅看数据传输成功。
3. 确认观察的是当前 bitstream 连接的 GPIO LED。
4. 音频先用 `SimpleTone` / `SinePolling`，确认输出引脚、地和模拟链路；`Synth.setVolume(0..255)` 只调数字幅度。
5. 区分编译成功、CPU 运行、数字波形正确和实际模拟声音，这几步需要各自的证据。
