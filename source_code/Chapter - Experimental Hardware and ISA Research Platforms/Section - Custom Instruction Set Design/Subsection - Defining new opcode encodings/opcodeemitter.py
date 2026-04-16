#!/usr/bin/env python3
# Emit 32-bit instructions for layout: OP(7)|SUB(3)|RD(5)|RS1(5)|RS2_IMM(12)
from dataclasses import dataclass

@dataclass(frozen=True)
class InstrLayout:
    op: int   # 7 bits
    sub: int  # 3 bits
    rd: int   # 5 bits
    rs1: int  # 5 bits
    rs2_imm: int  # 12 bits

    def pack(self) -> bytes:
        # Compose into 32-bit word, MSB..LSB as described
        if not (0 <= self.op < (1 << 7)):
            raise ValueError("op out of range")
        if not (0 <= self.sub < (1 << 3)):
            raise ValueError("sub out of range")
        if not (0 <= self.rd < (1 << 5)):
            raise ValueError("rd out of range")
        if not (0 <= self.rs1 < (1 << 5)):
            raise ValueError("rs1 out of range")
        # Allow signed 12-bit immediates; convert if negative
        imm = self.rs2_imm & ((1 << 12) - 1)
        word = (self.op << 25) | (self.sub << 22) | (self.rd << 17) | (self.rs1 << 12) | imm
        # Return little-endian bytes for memory emission
        return word.to_bytes(4, byteorder='little', signed=False)

# Example: encode ADD.RR with op=0x12, sub=0x3, rd=1, rs1=2, rs2=3
inst = InstrLayout(op=0x12, sub=0x3, rd=1, rs1=2, rs2_imm=3)
encoded = inst.pack()
print(encoded.hex())  # emitter can be used by assembler/linker pipeline