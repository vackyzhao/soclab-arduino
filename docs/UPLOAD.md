# 上传、CPU 启动及硬件适配

## 普通 Upload 的命令

```sh
riscv-none-elf-gdb -batch -nx \
  -ex "set confirm off" \
  -ex "target extended-remote 127.0.0.1:3333" \
  -ex "load" \
  -ex "monitor reset run" \
  -ex "detach" \
  YourSketch.ino.elf
```

- `target extended-remote` 连接已经运行的 OpenOCD。选择地址不会自动启动远端服务。
- `load` 按 ELF 段地址写入程序；内存如何写入由目标 OpenOCD 配置决定。
- `monitor reset run` 由 OpenOCD 复位并启动 CPU。ZedBoard 的启动路径需要完成 XIP 初始化，KR260 的 BRAM 程序则由它的复位启动地址进入。
- `detach` 断开调试连接。不要用一个额外的 GDB 连接测试“是否已运行”，因为连接调试器可能再次暂停 CPU。

## Local / Remote

在 **工具 → OpenOCD target** 选择 Local 或 Remote。远端地址在当前平台目录旁的 `boards.local.txt` 中设置，例如：

```ini
zedboard.menu.openocd.remote=Remote lab PC
zedboard.menu.openocd.remote.tools.gdb.server=192.0.2.1:3333
kr260.menu.openocd.remote=Remote KR260
kr260.menu.openocd.remote.tools.gdb.server=192.0.2.2:3333
```

以上是示例地址，请换成自己的局域网或 Tailscale IP。修改后重新打开 IDE。平台目录为手动安装时的 `<sketchbook>/hardware/soclab/neorv32`，或 Boards Manager 的 `<Arduino data>/packages/soclab/hardware/neorv32/<版本>`。

先配置上面的远端地址，然后使用 Arduino CLI：

```sh
arduino-cli compile --fqbn soclab:neorv32:kr260:build=board,openocd=remote --output-dir build ./Blink
arduino-cli upload --fqbn soclab:neorv32:kr260:build=board,openocd=remote --input-dir build ./Blink
```

`compile` 生成程序，`upload` 只上传已经生成的文件。IDE 与 CLI 共用 `boards.local.txt` 配置。

## 两种硬件布局

| 目标 | CPU / DMEM | 程序地址 | 程序存储 | 音频 |
|---|---|---|---|---|
| 课程 ZedBoard Lab3 | NEORV32 v1.11，约 98.304 MHz / 16 KiB | `0xE0000000` | XIP SPI flash，平台限制 64 KiB | 匹配课程扩展板与约束 |
| KR260 clean Lab3 r6 | 同一软件外设布局 / 16 KiB | `0xE0000000` | 可写 BRAM，128 KiB；重新加载 PL 或断电后丢失 | PMOD1/J2 物理 1=左、3=右；9/10=GND |

音频 MMIO 基址为 `0xFFD00000`，24 bit 有符号左右声道，FIFO 256 帧，采样率名义 48 kHz。`LED_BUILTIN` 对应 SoC GPIO20；KR260 r6 对应 UF1，GPIO21 对应 UF2。这是特定 bitstream 的映射，不是任意 KR260 工程的通用引脚表。

KR260 r6 将 PMOD J2 的上述两根脚设为输出，J21 不作音频输出。输出为 3.3 V 逻辑的 ΣΔ 位流，需匹配的模拟滤波及放大路径，不能把一个未滤波的逻辑引脚当作已验证的耳机输出。

## OpenOCD 和串口

ZedBoard 的 `tools/openocd_neorv32_jtaghs2.cfg` 针对课程扩展板的 CPU JTAG 与 XIP flash。它不适用于把 FPGA 配置 JTAG 自动当成 CPU JTAG。

KR260 clean Lab3 使用 PS 到 AXI GPIO 的 CPU JTAG 桥，并在 PS 上运行 OpenOCD；PS 侧桥接程序、设备树和 bitstream 属于硬件工程，不随此 Arduino core 安装。KR260 不能使用 ZedBoard 的 **Upload Using Programmer** SPI flash 流程，应使用已经配置好的远端 OpenOCD 和普通 **Upload**。

`Serial` 是 NEORV32 UART。KR260 的 UART 经 PS 侧 AXI16550 成为 Linux ttyS 设备；GDB/Tailscale 地址不等于一个 Windows COM 口。需要在 PS 上读写相应串口，或另行配置串口转发。

## 没闪灯／没声音时

1. 确认使用 Board 实时时钟模式，且已配置对应 FPGA bitstream。
2. 观察上传日志是否包含 `monitor reset run`，不是仅 `load` 成功。
3. 确认观察的是当前 bitstream 连接的 GPIO LED。
4. 音频先用 `SimpleTone` / `SinePolling`，确认输出引脚、地和模拟链路；`Synth.setVolume(0..255)` 只调数字幅度。
5. 区分编译成功、CPU 运行、数字波形正确和实际模拟声音，这几步需要各自的证据。
