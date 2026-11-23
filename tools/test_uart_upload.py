"""Native NEORV32 upload dialogue tests; no port or board is opened."""
import struct
import tempfile
import unittest
from pathlib import Path

from uart_upload import (AWAITING, PROMPT, SIGNATURE, SSH_SERIAL_BRIDGE, DT_RESET_CHECK,
                         UploadError, make_executable, upload)


class NativeLoader:
    def __init__(self, reply=b"OK", address=0xE0000000, fragment=7):
        self.reply, self.address, self.fragment = reply, address, fragment
        self.received = bytearray()
        self.output = bytearray()
        self.commands = []
        self.uploading = False
        self.image = None

    def write(self, data):
        if self.uploading:
            self.received.extend(data)
            if len(self.received) >= 12:
                signature, size, checksum = struct.unpack_from("<III", self.received)
                if len(self.received) == 12 + size:
                    assert signature == SIGNATURE
                    assert (sum(w[0] for w in struct.iter_unpack("<I", self.received[12:])) + checksum) & 0xFFFFFFFF == 0
                    self.image = bytes(self.received)
                    self.output.extend(self.reply + b"\r\n" + PROMPT)
                    self.uploading = False
            return
        self.commands.append(data)
        if data == b"h":
            self.output.extend(b"h\r\nAvailable CMDs: u: Upload e: Execute\r\n" + PROMPT)
        elif data == b"u":
            self.uploading = True
            self.output.extend(b"u\r\n" + AWAITING)
        elif data == b"e":
            assert self.image is not None
            self.output.extend(b"e\r\nBooting from 0x%08x...\r\n" % self.address)
        else:
            raise AssertionError("unexpected bytes before upload: " + repr(data))

    def read(self, size):
        data = bytes(self.output[:min(size, self.fragment)])
        del self.output[:len(data)]
        return data

    def flush(self):
        pass


class UploadTests(unittest.TestCase):
    def test_native_header_and_padding(self):
        image = make_executable(b"\x01\x02\x03\x04\x05", 8)
        self.assertEqual(image, struct.pack("<III", 0x4788CAFE, 8, 0xFBFCFDFA) +
                         b"\x01\x02\x03\x04\x05\0\0\0")

    def test_empty_and_capacity_errors(self):
        with self.assertRaisesRegex(UploadError, "empty"):
            make_executable(b"")
        with self.assertRaisesRegex(UploadError, "target program RAM"):
            make_executable(bytes(9), 8)

    def test_fragmented_success_and_actual_payload(self):
        loader = NativeLoader(fragment=1)
        executable = make_executable(bytes(range(256)) * 4)
        transcript = upload(loader, executable, timeout=0.2)
        self.assertEqual(loader.commands, [b"h", b"u", b"e"])
        self.assertEqual(loader.image, executable)
        self.assertIn(b"Booting from 0xe0000000", transcript)

    def test_ok_and_prompt_in_one_read(self):
        loader = NativeLoader(fragment=4096)
        upload(loader, make_executable(b"1234"), timeout=0.2)
        self.assertEqual(loader.commands[-1], b"e")

    def test_native_error_prevents_execute(self):
        for error in (b"ERR_EXE", b"ERR_SIZE", b"ERR_CHKS", b"ERR_EXC"):
            with self.subTest(error=error):
                loader = NativeLoader(reply=b"\a\r\n" + error, fragment=1)
                with self.assertRaisesRegex(UploadError, error.decode()):
                    upload(loader, make_executable(b"1234"), timeout=0.2)
                self.assertNotIn(b"e", loader.commands)

    def test_wrong_jump_address(self):
        loader = NativeLoader(address=0)
        with self.assertRaisesRegex(UploadError, "executes at 0x00000000"):
            upload(loader, make_executable(b"1234"), timeout=0.2)

    def test_no_prompt_sends_no_binary(self):
        loader = NativeLoader()
        loader.read = lambda size: b""
        with self.assertRaisesRegex(UploadError, "CMD:> prompt"):
            upload(loader, make_executable(b"1234"), timeout=0.01)
        self.assertEqual(loader.commands, [b"h"])
        self.assertEqual(loader.received, b"")

    def test_remote_helper_parses_without_execution(self):
        compile(SSH_SERIAL_BRIDGE, "<ssh-serial-bridge>", "exec")


class ResetDeviceTreeTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.parent = Path(self.temp.name) / 'axi'
        self.node = self.parent / 'cpu-reset@a0030000'
        self.node.mkdir(parents=True)
        self.namespace = {}
        exec(DT_RESET_CHECK, self.namespace)
        self.check_reg = self.namespace['check_reset_node_reg']

    def property(self, name, *values):
        (self.parent / name).write_bytes(struct.pack('>' + 'I' * len(values), *values))

    def reg(self, *values):
        (self.node / 'reg').write_bytes(struct.pack('>' + 'I' * len(values), *values))

    def test_32bit_parent_with_non_amba_name(self):
        self.property('#address-cells', 1)
        self.property('#size-cells', 1)
        self.reg(0xA0030000, 0x10000)
        self.check_reg(self.node, 0xA0030000)

    def test_64bit_parent_and_default_cells(self):
        self.property('#address-cells', 2)
        self.property('#size-cells', 2)
        self.reg(0, 0xA0030000, 0, 0x10000)
        self.check_reg(self.node, 0xA0030000)
        (self.parent / '#address-cells').unlink()
        (self.parent / '#size-cells').unlink()
        self.reg(0, 0xA0030000, 0x10000)
        self.check_reg(self.node, 0xA0030000)

    def test_wrong_address_or_too_small_range(self):
        self.property('#address-cells', 1)
        self.property('#size-cells', 1)
        for address, size in ((0xA0010000, 0x10000), (0xA0030000, 4)):
            with self.subTest(address=address, size=size):
                self.reg(address, size)
                with self.assertRaisesRegex(RuntimeError, 'reg mismatch'):
                    self.check_reg(self.node, 0xA0030000)

    def test_missing_or_malformed_properties_stop(self):
        with self.assertRaisesRegex(RuntimeError, 'Cannot read CPU-reset'):
            self.check_reg(self.node, 0xA0030000)
        self.property('#address-cells', 1)
        self.property('#size-cells', 1)
        self.reg(0xA0030000)
        with self.assertRaisesRegex(RuntimeError, 'one address/size range'):
            self.check_reg(self.node, 0xA0030000)
        self.property('#address-cells', 3)
        with self.assertRaisesRegex(RuntimeError, 'Unsupported'):
            self.check_reg(self.node, 0xA0030000)
        (self.parent / '#address-cells').write_bytes(b'\x01')
        with self.assertRaisesRegex(RuntimeError, 'Malformed'):
            self.check_reg(self.node, 0xA0030000)


if __name__ == "__main__":
    unittest.main()
