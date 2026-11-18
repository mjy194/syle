import unittest
from analyze_persistent_disk import phase_check
TOKEN='0123456789abcdef0123456789abcdef'
class PersistentEvidence(unittest.TestCase):
    def fixture(self,phase='write'):
        action='WRITTEN' if phase=='write' else 'VERIFIED';commands=[f'persistctl {phase} {TOKEN}\r','persistctl reboot\r']
        tx=('SYLE_DISK_EFI_LOADED\nSYLE_DISK_FILE_VERIFIED\nSYLE_DISK_LINUX_START_IMAGE\n'
            'Linux version 6.12.0-syle-root1\nvirtio_blk virtio0: [vda] 131072\nvda: vda1 vda2\n'
            'VFS: Mounted root (ext2 filesystem) on device 254:2.\nRun /sbin/init as init process\n'
            'SYLE_DISK_ROOT_READY\n/dev/root / ext2 rw,relatime 0 0\n'
            f'# persistctl {phase} {TOKEN}\nSYLE_PERSIST_{action} {TOKEN}\n# persistctl reboot\n'
            'SYLE_PERSIST_ROOT_REMOUNTED_RO\nSYLE_PERSIST_REBOOT_REQUEST\nreboot: machine restart\n')
        text=('OVMF.PERSIST guest_reset_requested backing_flushed1\nOVMF.DISK file_probe_complete\n'
              'OVMF.TRACE dump_end failed=0x0 incomplete=0x0\ndropped_bytes=0x0\n'
              'OVMF.PERSIST stats reads=0xa writes=0x2 flushes=0x2 irq5=0x3 reboot=0x1\n'
              'OVMF.DISK terminal bar=0xc000 command=0x5 status=0x7 pfn=0x1 notifies=0x5 completions=0x5 failed=0x0 backend_failed=0x0\n'
              f'OVMF.DISK PIC receipts=0x6 irq0=0x2 irq4=0x1 rx_sent={sum(map(len,commands)):#x} rx_read={sum(map(len,commands)):#x} active=0x1 IRR=0x0 ISR=0x0 mask=0xff\n'
              f'OVMF.CONSOLE terminal bytes={len(tx):#x} received=0x0 reads=0x0 writes=0x0 bar=0xc040 command=0x1 failed=0x0\n')
        text+=''.join(f'OVMF.CONSOLE TX byte={b:#x}\n' for b in tx.encode())
        result={'admission_graph_equal':True,'admitted_extension':'PIO','continued_once':True,'returncode':35,'backing_identity':{'bytes':67108864}}
        return text,commands,result
    def check(self,text,cmd,res,phase='write'):return phase_check(text.encode(),cmd,res,phase,TOKEN)
    def test_write(self):self.assertEqual(self.check(*self.fixture())[0]['phase'],'write')
    def test_read(self):self.assertEqual(self.check(*self.fixture('read'),phase='read')[0]['phase'],'read')
    def test_missing_irq(self):
        text,c,r=self.fixture()
        with self.assertRaises(AssertionError):self.check(text.replace('irq5=0x3','irq5=0x0'),c,r)
    def test_no_actual_reset(self):
        text,c,r=self.fixture()
        with self.assertRaises(AssertionError):self.check(text.replace('reboot=0x1','reboot=0x0'),c,r)
    def test_missing_write_result(self):
        text,c,r=self.fixture();needle=''.join(f'OVMF.CONSOLE TX byte={b:#x}\n' for b in ('SYLE_PERSIST_WRITTEN '+TOKEN+'\n').encode())
        with self.assertRaises(AssertionError):self.check(text.replace(needle,''),c,r)
    def test_capture_loss(self):
        text,c,r=self.fixture()
        with self.assertRaises(AssertionError):self.check(text.replace('OVMF.CONSOLE TX byte=0x53\n','',1),c,r)
    def test_timeout(self):
        text,c,r=self.fixture();r['timeout']=True
        with self.assertRaises(AssertionError):self.check(text,c,r)
    def test_trace_failure(self):
        text,c,r=self.fixture()
        with self.assertRaises(AssertionError):self.check(text.replace('incomplete=0x0','incomplete=0x1'),c,r)
if __name__=='__main__':unittest.main()
