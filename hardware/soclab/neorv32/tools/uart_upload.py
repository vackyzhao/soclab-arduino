#!/usr/bin/env python3
"""Upload a raw Arduino .bin with the native NEORV32 UART u/e protocol.

The executable format follows NEORV32 sw/image_gen/image_gen.c (-app_bin).
The loader must already be running unless explicit KR260 PS reset is selected.
This tool does not load a loader through GDB or program SPI flash.
"""
import argparse
import queue
import re
import shlex
import struct
import subprocess
import sys
import threading
import time
from pathlib import Path

SIGNATURE = 0x4788CAFE
PROMPT = b"CMD:> "
AWAITING = b"Awaiting neorv32_exe.bin... "
LOADER_ERROR = re.compile(rb"ERR_(?:EXE|SIZE|CHKS|SPI|TWI|EXC)|No executable\.|Invalid CMD")

# The bridge executes this small parser before opening /dev/mem. Kept as a
# separate source fragment so its actual remote implementation is testable
# against a temporary device-tree directory without opening hardware.
DT_RESET_CHECK = r'''
def check_reset_node_reg(node, reset_addr):
    def cells(name, default):
        path = node.parent / name
        if not path.exists():
            return default
        data = path.read_bytes()
        if len(data) != 4:
            raise RuntimeError('Malformed CPU-reset parent %s; no /dev/mem access attempted' % name)
        count = int.from_bytes(data, 'big')
        if count not in (1, 2):
            raise RuntimeError('Unsupported CPU-reset parent %s=%d; no /dev/mem access attempted' % (name, count))
        return count
    try:
        # Device-tree defaults when the immediate bus omits these properties.
        address_cells = cells('#address-cells', 2)
        size_cells = cells('#size-cells', 1)
        data = (node / 'reg').read_bytes()
        address_bytes = address_cells * 4
        expected = (address_cells + size_cells) * 4
        if len(data) != expected:
            raise RuntimeError('CPU-reset reg must contain one address/size range; no /dev/mem access attempted')
        address = int.from_bytes(data[:address_bytes], 'big')
        size = int.from_bytes(data[address_bytes:], 'big')
        if address != reset_addr or size < 8:
            raise RuntimeError('CPU-reset reg mismatch: address=0x%x size=0x%x, expected address=0x%x and at least 8 bytes; no /dev/mem access attempted' % (address, size, reset_addr))
    except OSError as exc:
        raise RuntimeError('Cannot read CPU-reset reg/cell properties; no /dev/mem access attempted') from exc
'''

# SSH carries UART bytes unchanged: stdout is the byte stream, stderr errors.
# The optional reset addresses an independent PS AXI GPIO, not the JTAG GPIO.
SSH_SERIAL_BRIDGE = DT_RESET_CHECK + r'''
import fcntl, mmap, os, select, subprocess, sys, termios, time
from pathlib import Path
device, baud = sys.argv[1], int(sys.argv[2])
reset_addr = int(sys.argv[3], 0) if len(sys.argv) > 3 else None
app_name = sys.argv[4] if len(sys.argv) > 4 else 'lab3_kr260_uart'
speed = getattr(termios, "B" + str(baud))
lock = None
fd = None
try:
    if reset_addr is not None:
        lock = open('/run/kr260_rv_mmio.lock', 'a+')
        try:
            fcntl.flock(lock.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError as exc:
            raise RuntimeError('MMIO is in use or PL is being reloaded; stop lab3-openocd and lab3-jtag before UART reset/upload') from exc
        # Same loaded-app and PL-clock checks as remote_bitbang_gpio.py:
        # probing an absent/stopped AXI slave through MMIO can hang the PS.
        result = subprocess.run(['xmutil', 'listapps'], capture_output=True,
                                text=True, timeout=30, check=True)
        active = [line.split()[0] for line in result.stdout.splitlines()
                  if line.split() and '->' in line]
        if app_name not in active:
            raise RuntimeError('%s is not the loaded PL app (active: %s)' %
                               (app_name, ', '.join(active) or 'none'))
        state = Path('/sys/class/fpga_manager/fpga0/state').read_text().strip()
        rate = int(Path('/sys/kernel/debug/clk/pl0_ref/clk_rate').read_text().strip())
        enabled = int(Path('/sys/kernel/debug/clk/pl0_ref/clk_enable_count').read_text().strip())
        if state != 'operating' or enabled < 1 or abs(rate - 100000000) > 1000000:
            raise RuntimeError('PL is not ready: state=%s, pl0_ref=%d Hz, enable=%d; no /dev/mem access attempted' %
                               (state, rate, enabled))
        node_name = 'cpu-reset@%x' % reset_addr
        nodes = list(Path('/sys/firmware/devicetree/base').rglob(node_name))
        if len(nodes) != 1 or b'soclab,kr260-cpu-reset' not in (nodes[0] / 'compatible').read_bytes().split(b'\0'):
            raise RuntimeError('Matching CPU-reset device-tree node is absent; no /dev/mem access attempted')
        check_reset_node_reg(nodes[0], reset_addr)
        if sys.byteorder != 'little':
            raise RuntimeError('KR260 register map requires little-endian PS Linux')
    fd = os.open(device, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    fcntl.ioctl(fd, termios.TIOCEXCL)
    cfg = termios.tcgetattr(fd)
    cfg[0], cfg[1], cfg[3] = 0, 0, 0
    cfg[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
    cfg[4], cfg[5] = speed, speed
    cfg[6][termios.VMIN], cfg[6][termios.VTIME] = 0, 0
    termios.tcsetattr(fd, termios.TCSANOW, cfg)
    termios.tcflush(fd, termios.TCIOFLUSH)
    if reset_addr is not None:
        memfd = os.open('/dev/mem', os.O_RDWR | os.O_SYNC)
        try:
            base = reset_addr & ~(mmap.PAGESIZE - 1)
            offset = reset_addr - base
            with mmap.mmap(memfd, mmap.PAGESIZE, flags=mmap.MAP_SHARED,
                           prot=mmap.PROT_READ | mmap.PROT_WRITE, offset=base) as mm:
                words = memoryview(mm).cast('I')
                try:
                    index = offset // 4
                    old, tri = words[index], words[index + 1]
                    words[index] = old | 1
                    words[index + 1] = tri & ~1
                    _ = words[index]
                    try:
                        time.sleep(0.05)
                        termios.tcflush(fd, termios.TCIOFLUSH)
                    finally:
                        words[index] = old & ~1
                        _ = words[index]
                finally:
                    words.release()
        finally:
            os.close(memfd)
    pending = bytearray()
    while True:
        readable, writable, _ = select.select([0, fd], [fd] if pending else [], [])
        if 0 in readable:
            data = os.read(0, 4096)
            if not data:
                break
            pending.extend(data)
        if fd in writable:
            try:
                n = os.write(fd, pending)
                del pending[:n]
            except BlockingIOError:
                pass
        if fd in readable:
            try:
                data = os.read(fd, 4096)
                if data:
                    os.write(1, data)
            except BlockingIOError:
                pass
finally:
    if fd is not None:
        os.close(fd)
    if lock is not None:
        lock.close()
'''


class UploadError(RuntimeError):
    pass


def make_executable(raw, max_size=131072):
    """Add native signature/size/complement-sum header; zero-pad to 32 bits.

    No load address is encoded: EXE_BASE_ADDR belongs to the bootloader build.
    """
    if not raw:
        raise UploadError("Program .bin is empty")
    payload = raw + bytes((-len(raw)) % 4)
    if len(payload) > max_size:
        raise UploadError("Program needs %d bytes; target program RAM is %d bytes" %
                          (len(payload), max_size))
    checksum = (-sum(word[0] for word in struct.iter_unpack("<I", payload))) & 0xFFFFFFFF
    return struct.pack("<III", SIGNATURE, len(payload), checksum) + payload


class SerialTransport:
    def __init__(self, port, baud):
        try:
            import serial
        except ImportError as exc:
            raise UploadError("Local UART upload needs pyserial (python -m pip install pyserial)") from exc
        self.port = serial.Serial(port=None, baudrate=baud, bytesize=8, parity="N",
                                  stopbits=1, timeout=0.1, write_timeout=10,
                                  xonxoff=False, rtscts=False, dsrdtr=False)
        # DTR/RTS are not CPU reset pins on these boards.
        self.port.dtr = False
        self.port.rts = False
        self.port.port = port
        self.port.open()

    def read(self, size):
        return self.port.read(min(size, max(self.port.in_waiting, 1)))

    def write(self, data):
        count = self.port.write(data)
        if count != len(data):
            raise UploadError("Serial write stopped after %d of %d bytes" % (count, len(data)))

    def flush(self):
        self.port.flush()

    def close(self):
        self.port.close()


class SSHTransport:
    def __init__(self, host, device, baud, identity=None, reset_address=None, sudo=False,
                 app_name='lab3_kr260_uart'):
        command = "python3 -u -c %s %s %d" % (shlex.quote(SSH_SERIAL_BRIDGE),
                                               shlex.quote(device), baud)
        if reset_address is not None:
            command += " " + hex(reset_address) + " " + shlex.quote(app_name)
        if sudo:
            command = "sudo -n " + command
        argv = ["ssh", "-T", "-o", "BatchMode=yes", "-o", "ConnectTimeout=10"]
        if identity:
            argv += ["-i", str(identity)]
        argv += [host, command]
        self.process = subprocess.Popen(argv, stdin=subprocess.PIPE,
                                        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                        bufsize=0)
        self.received = queue.Queue()
        self.pending = bytearray()
        self.errors = bytearray()
        threading.Thread(target=self._read_stdout, daemon=True).start()
        self.stderr_thread = threading.Thread(target=self._read_stderr, daemon=True)
        self.stderr_thread.start()

    def _read_stdout(self):
        while True:
            data = self.process.stdout.read(4096)
            self.received.put(data)
            if not data:
                return

    def _read_stderr(self):
        while True:
            data = self.process.stderr.read(4096)
            if not data:
                return
            self.errors.extend(data)

    def read(self, size):
        if not self.pending:
            try:
                data = self.received.get(timeout=0.1)
            except queue.Empty:
                if self.process.poll() is not None:
                    self._closed()
                return b""
            if not data:
                self.process.wait(timeout=2)
                self._closed()
            self.pending.extend(data)
        data = bytes(self.pending[:size])
        del self.pending[:size]
        return data

    def _closed(self):
        self.stderr_thread.join(timeout=0.2)
        detail = self.errors.decode("utf-8", errors="replace").strip()
        raise UploadError("SSH serial bridge closed (%s): %s" %
                          (self.process.returncode, detail))

    def write(self, data):
        try:
            self.process.stdin.write(data)
            self.process.stdin.flush()
        except (BrokenPipeError, OSError) as exc:
            detail = self.errors.decode("utf-8", errors="replace").strip()
            raise UploadError("Cannot write SSH serial bridge: " + detail) from exc

    def flush(self):
        # Native OK confirms receipt; flushing this pipe alone does not.
        self.process.stdin.flush()

    def close(self):
        try:
            self.process.stdin.close()
        except OSError:
            pass
        try:
            self.process.wait(timeout=2)
        except subprocess.TimeoutExpired:
            self.process.terminate()
            try:
                self.process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait(timeout=2)


class LoaderReader:
    def __init__(self, transport):
        self.transport = transport
        self.pending = bytearray()
        self.transcript = bytearray()

    def until(self, expected, phase, timeout):
        pattern = re.compile(re.escape(expected)) if isinstance(expected, bytes) else expected
        deadline = time.monotonic() + timeout
        while True:
            error = LOADER_ERROR.search(self.pending)
            if error:
                raise UploadError("Bootloader rejected %s: %s" %
                                  (phase, error.group().decode("ascii", errors="replace")))
            match = pattern.search(self.pending)
            if match:
                data = bytes(self.pending[:match.end()])
                del self.pending[:match.end()]
                return data
            if time.monotonic() >= deadline:
                tail = bytes(self.pending[-240:]).decode("ascii", errors="replace")
                raise UploadError("Timed out waiting for %s. Reset into the UART bootloader "
                                  "and check the baud/port. Received: %r" % (phase, tail))
            data = self.transport.read(4096)
            self.pending.extend(data)
            self.transcript.extend(data)


def upload(transport, executable, baud=115200, timeout=10, expected_address=0xE0000000,
           report=lambda message: None):
    reader = LoaderReader(transport)
    # Request a fresh native prompt; h also aborts an active autoboot countdown.
    transport.write(b"h")
    reader.until(PROMPT, "CMD:> prompt", timeout)
    transport.write(b"u")
    reader.until(AWAITING, "upload request", timeout)
    report("NEORV32 UART: sending %d program bytes at %d baud" %
           (len(executable) - 12, baud))
    for offset in range(0, len(executable), 256):
        transport.write(executable[offset:offset + 256])
    transport.flush()
    reader.until(b"OK", "image checksum acceptance", timeout + len(executable) * 10 / baud)
    reader.until(PROMPT, "post-upload CMD:> prompt", timeout)
    report("NEORV32 UART: loader accepted size and checksum; executing application")
    transport.write(b"e")
    boot = reader.until(re.compile(rb"Booting from 0x([0-9a-fA-F]{8})\.\.\."),
                        "execution announcement", timeout)
    address = int(re.search(rb"0x([0-9a-fA-F]{8})", boot).group(1), 16)
    if address != expected_address:
        raise UploadError("Bootloader executes at 0x%08x; this image expects 0x%08x" %
                          (address, expected_address))
    report("NEORV32 UART: Booting from 0x%08x (no separate GDB run command)" % address)
    return bytes(reader.transcript)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bin", required=True, type=Path, help="raw Arduino objcopy .bin")
    endpoint = parser.add_mutually_exclusive_group()
    endpoint.add_argument("--port", help="local serial port: COM6 or /dev/ttyUSB0")
    endpoint.add_argument("--ssh", help="SSH target with the Linux PS CPU UART0 device")
    parser.add_argument("--device", default="/dev/ttyS0", help="remote PS AXI UART device")
    parser.add_argument("--identity", type=Path, help="SSH private-key filename")
    parser.add_argument("--reset-address", type=lambda value: int(value, 0),
                        help="explicit SSH-only CPU reset AXI GPIO base; bit0 1=assert, 0=release")
    parser.add_argument("--app-name", default="lab3_kr260_uart", help="required loaded xmutil app for an explicit PS reset")
    parser.add_argument("--ssh-sudo", action="store_true", help="use existing noninteractive sudo for remote helper")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=10)
    parser.add_argument("--max-size", type=lambda value: int(value, 0), default=131072)
    parser.add_argument("--expect-address", type=lambda value: int(value, 0), default=0xE0000000,
                        help="expected bootloader jump; not a native-header field")
    parser.add_argument("--make-exe", type=Path, help="also write native executable; alone, package offline")
    args = parser.parse_args(argv)
    if args.baud <= 0 or args.timeout <= 0 or args.max_size <= 0:
        parser.error("baud, timeout and max-size must be positive")
    if not (args.port or args.ssh or args.make_exe):
        parser.error("choose --port, --ssh, or offline --make-exe")
    if (args.reset_address is not None or args.ssh_sudo) and not args.ssh:
        parser.error("--reset-address and --ssh-sudo require --ssh")
    raw = args.bin.read_bytes()
    if raw[:4] == b"\x7fELF":
        raise UploadError("Pass the objcopy .bin, not an ELF")
    if len(raw) >= 4 and struct.unpack_from("<I", raw)[0] == SIGNATURE:
        raise UploadError("Input already has a NEORV32 header; pass the raw Arduino .bin")
    executable = make_executable(raw, args.max_size)
    if args.make_exe:
        args.make_exe.write_bytes(executable)
        print("Wrote native NEORV32 executable:", args.make_exe, flush=True)
    if not (args.port or args.ssh):
        return 0
    transport = (SerialTransport(args.port, args.baud) if args.port else
                 SSHTransport(args.ssh, args.device, args.baud, args.identity,
                              args.reset_address, args.ssh_sudo, args.app_name))
    try:
        upload(transport, executable, args.baud, args.timeout, args.expect_address,
               lambda message: print(message, flush=True))
    finally:
        transport.close()
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (UploadError, OSError, subprocess.SubprocessError) as exc:
        print("uart_upload:", exc, file=sys.stderr)
        raise SystemExit(1)
