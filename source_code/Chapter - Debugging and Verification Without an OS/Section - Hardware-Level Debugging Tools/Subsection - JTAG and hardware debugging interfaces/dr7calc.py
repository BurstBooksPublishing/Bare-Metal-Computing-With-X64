#!/usr/bin/env python3
# dr7calc.py -- compute DR7 for hardware breakpoints (x86/x86_64, Intel encoding)

from typing import NamedTuple, Sequence

class BP(NamedTuple):
    index: int            # 0..3
    local: bool           # local enable (per-processor)
    rw: int               # RW code: 0=exec,1=write,2=io,3=read/write
    length: int           # LEN code: 0=1B,1=2B,2=8B,3=4B

def dr7_for(bps: Sequence[BP]) -> int:
    dr7 = 0
    for bp in bps:
        if not (0 <= bp.index <= 3):
            raise ValueError("index must be 0..3")
        # local enable bit position
        pos_le = 2 * bp.index
        if bp.local:
            dr7 |= (1 << pos_le)
        # RW and LEN positions
        pos_rw = 16 + 4 * bp.index
        pos_len = 18 + 4 * bp.index
        dr7 |= ((bp.rw & 0b11) << pos_rw)
        dr7 |= ((bp.length & 0b11) << pos_len)
    return dr7

# Example: enable local BP1 as read/write length=8 bytes
example = [BP(index=1, local=True, rw=3, length=2)]
print(f"DR7=0x{dr7_for(example):08x}")  # output can be written via JTAG/reg write