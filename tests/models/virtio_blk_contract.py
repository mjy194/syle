"""Host-only executable design contract, not a device/transport implementation.

No VMX, files-as-disks, DMA, guest writes, interrupt delivery or runtime imports.
All memory ranges are half-open. Clients supply immutable descriptor snapshots.
"""
from dataclasses import dataclass

U64 = (1 << 64) - 1
N = 128
NEXT, WRITE, INDIRECT = 1, 2, 4


class Invalid(ValueError):
    pass


def span(start, size):
    if not isinstance(start, int) or not isinstance(size, int):
        raise Invalid('noninteger span')
    if start < 0 or size <= 0 or start > U64 or size > U64 - start + 1:
        raise Invalid('overflow/empty span')
    return (start, start + size)


def overlap(a, b):
    return a[0] < b[1] and b[0] < a[1]


def admitted(start, size, ram):
    target = span(start, size)
    # Deliberately conservative: one admitted region must contain the range.
    if not any(lo <= target[0] and target[1] <= hi for lo, hi in ram):
        raise Invalid('nonowned DMA')
    return target


def allocation(pfn, ram):
    """Full legacy allocation (2.7.2), including reserved words and padding.

    This is an ownership admission policy, not permission to DMA into padding.
    """
    if not isinstance(pfn, int) or not 0 < pfn <= 0xffffffff:
        raise Invalid('disabled/invalid PFN')
    base = pfn << 12
    available_end = base + 16 * N + 2 * (3 + N)
    used_base = (available_end + 4095) & ~4095
    end = (used_base + 2 * 3 + 8 * N + 4095) & ~4095
    return admitted(base, end - base, ram)


def layout(pfn, ram):
    if not isinstance(pfn, int) or not 0 < pfn <= 0xffffffff:
        raise Invalid('disabled/invalid PFN')
    allocation(pfn, ram)
    base = pfn << 12
    desc = admitted(base, 16 * N, ram)
    avail = admitted(desc[1], 4 + 2 * N, ram)
    used_base = (avail[1] + 4095) & ~4095
    used = admitted(used_base, 4 + 8 * N, ram)
    return desc, avail, used


def pending(last, current):
    if not 0 <= last <= 65535 or not 0 <= current <= 65535:
        raise Invalid('invalid index')
    count = (current - last) & 65535
    if count > N:
        raise Invalid('producer overrun')
    return count


@dataclass(frozen=True)
class Descriptor:
    addr: int
    length: int
    flags: int = 0
    next: int = 0


def chain(table, head, ram, metadata):
    if len(table) != N:
        raise Invalid('table size')
    seen, result = set(), []
    index = head
    writable_seen, total = False, 0
    while True:
        if not isinstance(index, int) or not 0 <= index < N or index in seen:
            raise Invalid('index/loop')
        seen.add(index)
        d = table[index]
        if (not isinstance(d, Descriptor) or not isinstance(d.flags, int)
                or not 0 <= d.flags <= 65535 or not isinstance(d.next, int)
                or not 0 <= d.next <= 65535 or d.flags & ~(NEXT | WRITE)):
            raise Invalid('unsupported flags/descriptor')
        if not isinstance(d.length, int) or not 0 < d.length <= 0xffffffff:
            raise Invalid('descriptor length/strict nonzero subset')
        total += d.length
        if total > (1 << 32):
            raise Invalid('chain length exceeds 2^32')
        if writable_seen and not d.flags & WRITE:
            raise Invalid('readable after writable')
        writable_seen |= bool(d.flags & WRITE)
        region = admitted(d.addr, d.length, ram)
        if any(overlap(region, m) for m in metadata):
            raise Invalid('metadata alias')
        if d.flags & WRITE:
            if any(overlap(region, prior) for prior, _ in result):
                raise Invalid('writable alias')
        elif any(flags & WRITE and overlap(region, prior)
                 for prior, flags in result):
            raise Invalid('read/write alias')
        result.append((region, d.flags))
        if not d.flags & NEXT:
            return result
        index = d.next


@dataclass(frozen=True)
class Request:
    status: str
    data_bytes: int
    disk_offset: int | None
    used_length: int
    # Ordered data/status writes; includes status even when it is past a hole
    # and therefore is not counted in the contiguous valid prefix used_length.
    written_ranges: tuple = ()


def completion(regions, status, data_bytes=0, disk_offset=None):
    remaining, writes = data_bytes, []
    for (lo, hi), flags in regions[1:-1]:
        if remaining and flags & WRITE:
            count = min(remaining, hi - lo)
            writes.append((lo, lo + count))
            remaining -= count
    if remaining:
        raise Invalid('completion exceeds writable capacity')
    writes.append(regions[-1][0])
    # Count the valid prefix of concatenated writable buffers, not scattered
    # bytes written. Virtio 1.2 section 2.7.8.2 permits under-reporting only.
    prefix = 0
    for (lo, hi), flags in regions:
        if not flags & WRITE:
            continue
        cursor = lo
        for start, end in writes:
            if start <= cursor < end:
                cursor = min(hi, end)
        prefix += cursor - lo
        if cursor != hi:
            break
    return Request(status, data_bytes, disk_offset, prefix, tuple(writes))


def request(regions, kind, sector, capacity):
    """Classify already snapshotted header fields; never read/write guest RAM.

    Strict subset: header exactly 16 bytes, terminal status exactly one byte.
    Errors of structure are terminal; semantic block errors return status only.
    """
    if len(regions) < 2:
        raise Invalid('missing header/status')
    # Also validate public request() input; do not rely only on chain().
    writable_seen = False
    if sum(hi - lo for (lo, hi), _ in regions) > (1 << 32):
        raise Invalid('chain length exceeds 2^32')
    for _, flags in regions:
        if flags & ~(NEXT | WRITE) or (writable_seen and not flags & WRITE):
            raise Invalid('flags/readable after writable')
        writable_seen |= bool(flags & WRITE)
    header, hf = regions[0]
    status, sf = regions[-1]
    if header[1] - header[0] != 16 or hf & WRITE:
        raise Invalid('header layout')
    if status[1] - status[0] != 1 or not sf & WRITE or sf & NEXT:
        raise Invalid('status layout')
    if not 0 <= sector <= U64 or not 0 <= capacity <= U64 // 512:
        raise Invalid('capacity/sector')
    middle = regions[1:-1]
    length = sum(hi - lo for (lo, hi), _ in middle)
    # No used length truncation even if a hostile chain has huge buffers.
    if length > 0xfffffffe:
        raise Invalid('used length overflow')
    if kind in (0, 8):
        if any(not flags & WRITE for _, flags in middle):
            raise Invalid('read direction')
        if kind == 8:
            if length < 20:
                return completion(regions, 'IOERR')
            return completion(regions, 'OK', 20)
        if length == 0 or length % 512:
            return completion(regions, 'IOERR')
        if sector > capacity or length // 512 > capacity - sector:
            return completion(regions, 'IOERR')
        return completion(regions, 'OK', length, sector * 512)
    if kind == 1:
        if any(flags & WRITE for _, flags in middle):
            raise Invalid('write direction')
        return completion(regions, 'IOERR')
    return completion(regions, 'UNSUPP')
