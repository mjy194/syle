import unittest
from analyze_firmware_disk import check,COMMANDS

class DiskEvidence(unittest.TestCase):
    def fixture(self):
        tx=('SYLE_DISK_EFI_LOADED\nSYLE_DISK_FILE_VERIFIED\nSYLE_DISK_LINUX_START_IMAGE\n'
            'Linux version 6.12.0-syle-efi1\nRun /init as init process\nSYLE_LINUX_USERSPACE_READY\nBusyBox v1.37.0\n'
            '# uname -s\nLinux\n# uname -r\n6.12.0-syle-efi1\n'
            '# echo SYLE_LINUX_DISK_COMMAND_OK_20261118\nSYLE_LINUX_DISK_COMMAND_OK_20261118\n# ')
        n=sum(map(len,COMMANDS))
        text=('OVMF.DISK file_probe_complete\nOVMF.TRACE dump_end failed=0x0 incomplete=0x0\ndropped_bytes=0x0\n'
              'OVMF.DISK terminal bar=0xc000 command=0x5 status=0x0 pfn=0x0 notifies=0x24 completions=0x3a failed=0x0 backend_failed=0x0\n'
              f'OVMF.DISK PIC receipts=0xc irq0=0xa irq4=0x2 rx_sent={n:#x} rx_read={n:#x} active=0x1 IRR=0x0 ISR=0x0 mask=0xea\n')
        text+=f'OVMF.CONSOLE terminal bytes={len(tx):#x} received=0x0 reads=0x0 writes=0x0 bar=0xc040 command=0x1 failed=0x0\n'
        return text+''.join(f'OVMF.CONSOLE TX byte={b:#x}\n' for b in tx.encode()),tx
    def run_case(self,text,**kw):
        result={'admission_graph_equal':True,'continued_once':True,'returncode':35};result.update(kw)
        return check(text.encode(),COMMANDS,result)
    def test_positive(self):self.assertEqual(self.run_case(self.fixture()[0])[0]['validation'],'PASS')
    def test_incomplete(self):
        with self.assertRaises(AssertionError):self.run_case(self.fixture()[0].replace('incomplete=0x0','incomplete=0x1'))
    def test_disk_failure(self):
        with self.assertRaises(AssertionError):self.run_case(self.fixture()[0].replace('backend_failed=0x0','backend_failed=0x1'))
    def test_rx_not_read(self):
        t,_=self.fixture();n=sum(map(len,COMMANDS))
        with self.assertRaises(AssertionError):self.run_case(t.replace(f'rx_read={n:#x}','rx_read=0x0'))
    def test_timeout(self):
        with self.assertRaises(AssertionError):self.run_case(self.fixture()[0],timeout=True)
    def test_echo_is_not_result(self):
        text,tx=self.fixture();bad=tx.replace('\nSYLE_LINUX_DISK_COMMAND_OK_20261118\n','\n')
        text=text[:text.index('OVMF.CONSOLE TX byte=')]+''.join(f'OVMF.CONSOLE TX byte={b:#x}\n' for b in bad.encode())
        with self.assertRaises(AssertionError):self.run_case(text)
    def test_no_trace(self):
        with self.assertRaises(AssertionError):self.run_case(self.fixture()[0].replace('OVMF.TRACE dump_end','missing'))

if __name__=='__main__':unittest.main()
