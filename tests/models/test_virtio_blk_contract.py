"""Run: python -m unittest discover -s tests/models -p 'test_virtio_blk_contract.py'."""
import unittest
from virtio_blk_contract import *

RAM = [(0, 128 * 1024 * 1024)]


class Contract(unittest.TestCase):
    def fixture(self, data_len=512, data_flags=NEXT | WRITE):
        table = [None] * N
        table[0] = Descriptor(0x10000, 16, NEXT, 1)
        table[1] = Descriptor(0x11000, data_len, data_flags, 2)
        table[2] = Descriptor(0x12000, 1, WRITE)
        return table

    def parsed(self, table=None):
        return chain(self.fixture() if table is None else table, 0, RAM, layout(1, RAM))

    def test_layout_and_unused_event_fields(self):
        self.assertEqual(layout(1, RAM), ((4096,6144),(6144,6404),(8192,9220)))

    def test_legacy_reserved_allocation_is_not_dma_payload(self):
        self.assertEqual(allocation(1,RAM),(4096,12288))
        # Core fields fit, but ownership admission also covers reserved/padding.
        with self.assertRaises(Invalid): layout(1,[(4096,9220)])

    def test_pfn_disabled_large_or_hole(self):
        for pfn in (0,-1,1<<32,0xffffffff):
            with self.assertRaises(Invalid): layout(pfn,RAM)
        with self.assertRaises(Invalid): layout(1,[(4096,6404)])

    def test_unsigned_span_edges(self):
        self.assertEqual(span(U64,1),(U64,U64+1))
        for start, size in ((U64,2),(-1,1),(1,0),(1,-1)):
            with self.assertRaises(Invalid): span(start,size)

    def test_modular_indices(self):
        self.assertEqual(pending(65534,1),3)
        self.assertEqual(pending(1,129),N)
        for a,b in ((1,130),(1,0),(-1,0),(0,65536)):
            with self.assertRaises(Invalid): pending(a,b)

    def test_last_sector(self):
        self.assertEqual(request(self.parsed(),0,7,8),Request('OK',512,3584,513,((0x11000,0x11200),(0x12000,0x12001))))
        self.assertEqual(request(self.parsed(),0,8,8).status,'IOERR')
        self.assertEqual(request(self.parsed(),0,U64,8).status,'IOERR')

    def test_range_capacity_and_used_overflow(self):
        with self.assertRaises(Invalid): request(self.parsed(),0,0,U64)
        huge=[((0,16),NEXT),((16,16+0xffffffff),NEXT|WRITE),((U64-1,U64),WRITE)]
        with self.assertRaises(Invalid): request(huge,0,0,8)

    def test_cross_page_payload(self):
        t=self.fixture(); t[1]=Descriptor(0x10fff,512,NEXT|WRITE,2)
        self.assertEqual(request(self.parsed(t),0,0,1).status,'OK')

    def test_ro_write_no_disk_mutation(self):
        self.assertEqual(request(self.parsed(self.fixture(data_flags=NEXT)),1,0,8),Request('IOERR',0,None,1,((0x12000,0x12001),)))

    def test_get_id_unknown_flush(self):
        self.assertEqual(request(self.parsed(),8,0,8),Request('OK',20,None,20,((0x11000,0x11014),(0x12000,0x12001))))
        for kind in (4,11,13,0xffffffff):
            self.assertEqual(request(self.parsed(),kind,0,8),Request('UNSUPP',0,None,0,((0x12000,0x12001),)))

    def test_bad_direction_header_status(self):
        for index,desc in ((0,Descriptor(0x10000,16,NEXT|WRITE,1)),
                           (0,Descriptor(0x10000,15,NEXT,1)),
                           (1,Descriptor(0x11000,512,NEXT,2)),
                           (2,Descriptor(0x12000,1,0))):
            t=self.fixture();t[index]=desc
            with self.assertRaises(Invalid): request(self.parsed(t),0,0,8)

    def test_cycle_indirect_unknown_next(self):
        for desc in (Descriptor(0x11000,512,NEXT|WRITE,0),
                     Descriptor(0x11000,512,INDIRECT),
                     Descriptor(0x11000,512,8),
                     Descriptor(0x11000,512,NEXT|WRITE,N)):
            t=self.fixture();t[1]=desc
            with self.assertRaises(Invalid): self.parsed(t)

    def test_unowned_overflow_metadata_and_alias(self):
        for address,length in ((128*1024*1024,512),(U64,512),(4096,512),(0x10000,512),(0x12000,512)):
            t=self.fixture();t[1]=Descriptor(address,length,NEXT|WRITE,2)
            with self.assertRaises(Invalid): self.parsed(t)

    def test_scatter_read(self):
        t=self.fixture();t[1]=Descriptor(0x11000,256,NEXT|WRITE,3)
        t[3]=Descriptor(0x13000,256,NEXT|WRITE,2)
        self.assertEqual(request(self.parsed(t),0,0,1).used_length,513)

    def test_short_id_and_unaligned_read(self):
        for kind,length in ((8,19),(0,511)):
            self.assertEqual(request(self.parsed(self.fixture(length)),kind,0,1).status,'IOERR')

    def test_valid_prefix_not_sum_of_scattered_writes(self):
        for kind, length, sector in ((0,512,8),(0,511,0),(8,19,0)):
            r=request(self.parsed(self.fixture(length)),kind,sector,8)
            self.assertEqual(r.used_length,0)
            self.assertEqual(r.written_ranges,((0x12000,0x12001),))
        self.assertEqual(request(self.parsed(self.fixture(20)),8,0,8).used_length,21)
        self.assertEqual(request(self.parsed(self.fixture(21)),8,0,8).used_length,20)

    def test_unknown_readable_after_writable_rejected(self):
        t=self.fixture();t[1]=Descriptor(0x11000,1,NEXT|WRITE,3)
        t[3]=Descriptor(0x13000,1,NEXT,2)
        with self.assertRaises(Invalid): self.parsed(t)
        raw=[((0,16),NEXT),((32,33),NEXT|WRITE),((48,49),NEXT),((64,65),WRITE)]
        with self.assertRaises(Invalid): request(raw,99,0,8)

    def test_wire_length_and_chain_2pow32_limit(self):
        t=self.fixture();t[1]=Descriptor(0x100000,1<<32,NEXT|WRITE,2)
        with self.assertRaises(Invalid): chain(t,0,[(0,1<<40)],())
        t[1]=Descriptor(0x100000,0xffffffff,NEXT|WRITE,2)
        with self.assertRaises(Invalid): chain(t,0,[(0,1<<40)],())

    def test_status_only_unknown_has_valid_prefix_one(self):
        t=self.fixture();t[0]=Descriptor(0x10000,16,NEXT,2)
        r=request(self.parsed(t),99,0,8)
        self.assertEqual(r.used_length,1)
        self.assertEqual(r.status,'UNSUPP')


if __name__ == '__main__':
    unittest.main()
