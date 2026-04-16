# Minimal, dependency-free x86-64 encoder for bare-metal emitters.
import struct

def rex_byte(w=0, r=0, x=0, b=0):
    # Build REX prefix if any high-bit set or W requested.
    rex = 0x40 | ((w & 1) << 3) | ((r & 1) << 2) | ((x & 1) << 1) | (b & 1)
    return bytes([rex]) if rex != 0x40 else b''

def modrm_byte(mod, reg, rm):
    return bytes([(mod << 6) | ((reg & 7) << 3) | (rm & 7)])

def encode_mov_reg_imm64(reg, imm):
    # reg: 0..15, imm: 64-bit int
    rex = rex_byte(w=1, b=(reg >> 3) & 1)
    opcode = bytes([0xB8 + (reg & 7)])
    imm64 = struct.pack('<Q', imm)
    return rex + opcode + imm64

def encode_mov_reg_reg(src, dst):
    # mov r64, r64  opcode 0x89 /r  (r = src, r/m = dst)
    rex = rex_byte(w=1, r=(src >> 3) & 1, b=(dst >> 3) & 1)
    opcode = bytes([0x89])
    modrm = modrm_byte(0b11, src, dst)
    return rex + opcode + modrm

def encode_call_rel32(current_rip, target_rip):
    # E8 + rel32 where rel = target - (current + 5)
    rel = int(target_rip - (current_rip + 5))
    return b'\xE8' + struct.pack('<i', rel)