#!/usr/bin/env python3
# Production-ready verifier: requires 'capstone' package.
from capstone import Cs, CS_ARCH_X86, CS_MODE_64, x86_const
import struct, sys

FORBIDDEN_MNEMS = {'lgdt','lidt','wrmsr','invd','hlt'}  # example user-mode forbidden set

def is_canonical(addr):
    # signed 48-bit range check
    return - (1 << 47) <= (addr if addr < (1<<63) else addr - (1<<64)) <= (1 << 47) - 1

def disassemble_and_verify(code_bytes, base=0):
    cs = Cs(CS_ARCH_X86, CS_MODE_64)
    cs.detail = True
    starts = []
    instrs = {}
    # Disassemble sequentially and record starts -> length
    for ins in cs.disasm(code_bytes, base):
        starts.append(ins.address)
        instrs[ins.address] = ins
    starts_set = set(starts)

    # Boundary consistency: ensure contiguous, non-overlapping by checking next-start == addr + size
    for addr in starts:
        next_addr = addr + instrs[addr].size
        # If next_addr not in starts_set and not equal end, report potential hole or overlap
        # Note: last instruction next_addr may equal base+len(code_bytes)
        if next_addr not in starts_set and next_addr != base + len(code_bytes):
            raise SystemExit(f"Boundary error: instr at 0x{addr:x} size {instrs[addr].size} -> next 0x{next_addr:x} not aligned")

    # Branch target checks
    for addr, ins in instrs.items():
        mnem = ins.mnemonic.lower()
        if mnem in FORBIDDEN_MNEMS:
            raise SystemExit(f"Forbidden instruction {mnem} at 0x{addr:x}")
        # direct relative branches (jmp, je, call with immediate)
        if ins.group(x86_const.X86_GRP_JUMP) or ins.mnemonic.startswith('call'):
            for op in ins.operands:
                if op.type == x86_const.X86_OP_IMM:
                    target = op.imm
                    # verify target canonicality and that it is a listed instruction start
                    if not is_canonical(target):
                        raise SystemExit(f"Non-canonical target 0x{target:x} from 0x{addr:x}")
                    if target not in starts_set:
                        raise SystemExit(f"Branch target 0x{target:x} from 0x{addr:x} not an instruction start")
    return True

if __name__ == '__main__':
    # Usage: verifier.py  [base_hex]
    blob = open(sys.argv[1],'rb').read()
    base = int(sys.argv[2],0) if len(sys.argv)>2 else 0x1000
    disassemble_and_verify(blob, base=base)
    print("Verification passed")