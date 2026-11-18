"""Check rollover arithmetic and reject incomplete/contradictory evidence."""
import tempfile
import unittest
from pathlib import Path

from analyze_firmware_pm_delay import HZ, MASK, analyze_serial, analyze_sampled_serial


def read(tick):
    return ("OVMF.CLOCK_APIC exit reason=0x1e rip=0x70c231b qual=0xb008000b\n"
            f"OVMF.CLOCK_APIC pci_cycle port=0xb008 after_rax={tick:#x} accepted=0x1\n")


def pause(remaining):
    return ("OVMF.CLOCK_APIC exit reason=0x28 rip=0x70c236e "
            f"rax={remaining:#x}\nOVMF.PAUSE result=0x0 terminal=0x0 length=0x2 "
            "byte0=0xf3 byte1=0x90 unchanged_clock_and_exit_budgets\n")


class ProgressTests(unittest.TestCase):
    def analyze(self, text, sampled=False):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'serial.log'
            path.write_text(text)
            return (analyze_sampled_serial if sampled else analyze_serial)(path)

    def test_one_second_across_24_bit_rollover(self):
        start = MASK - 100
        result = self.analyze(read(start) + read((start + 50) & MASK) +
                              pause(HZ - 50) + read((start + 150) & MASK) +
                              pause(HZ - 150 + (1 << 24)))
        self.assertEqual(result['requested_chunk_ticks'], HZ)
        self.assertEqual(result['progress_ticks'], 150)
        self.assertEqual(result['remaining_ticks'], HZ - 150)
        self.assertEqual(result['requested_chunk_seconds'], 1)

    def test_inconsistent_target_rejected(self):
        with self.assertRaisesRegex(ValueError, 'stable wait target'):
            self.analyze(read(100) + pause(1000) + read(200) + pause(901))

    def test_backwards_read_rejected(self):
        with self.assertRaisesRegex(ValueError, 'backwards'):
            self.analyze(read(200) + pause(900) + read(100) + pause(1000))

    def test_sampled_log_cannot_be_treated_as_complete(self):
        with self.assertRaisesRegex(ValueError, 'missing PM read result'):
            self.analyze('OVMF.CLOCK_APIC exit reason=0x1e rip=0x70c231b qual=0xb008000b\n')

    def test_repeated_requests_and_rollover(self):
        text = ('OVMF.PM_DELAY snapshot pauses=0x1 target_ebx=0x1000010 '
                'request_edi=0x100 chunks_r12=0x0\n'
                'OVMF.PM_DELAY target_crossed target=0x1000010 returned=0x11\n'
                'OVMF.PM_DELAY snapshot pauses=0x100 target_ebx=0x111 '
                'request_edi=0x100 chunks_r12=0x0\n'
                'OVMF.PM_DELAY terminal pauses=0x180\n'
                'OVMF.RTC_OBSERVE phase=terminal_no_new_sample pm_ticks=0x1000100\n'
                'OVMF.CLOCK_APIC failed reason=0x1e rip=0x70c231c\n')
        result = self.analyze(text, sampled=True)
        self.assertEqual(result['completed_timer_conditions'], 1)
        self.assertTrue(result['waits'][0]['next_request_observed'])
        self.assertEqual(result['waits'][0]['overshoot_ticks'], 1)
        self.assertEqual(result['waits'][1]['remaining_ticks'], 17)
        # Equality still follows the guest's PAUSE path; it is not completion.
        with self.assertRaisesRegex(ValueError, 'exit condition'):
            self.analyze(text.replace('returned=0x11', 'returned=0x10'), sampled=True)


if __name__ == '__main__':
    unittest.main()
