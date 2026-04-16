import struct

# register low 3-bit codes
REG_CODE = {
    'rax':0,'rcx':1,'rdx':2,'rbx':3,'rsp':4,'rbp':5,'rsi':6,'rdi':7,
    'r8':0,'r9':1,'r10':2,'r11':3,'r12':4,'r13':5,'r14':6,'r15':7
}
# high bit if reg is r8..r15
def rex_bits(reg_name, W=1, R=0, X=0, B=0):
    reg_hi = 1 if reg_name.startswith('r') and int(reg_name[1:])>=8 else 0
    return (0x40 | (W<<3) | (R<<2) | (X<<1) | (B | reg_hi)) if (W or R or X or B or reg_hi) else b''

def emit_push(reg):
    # push r64: opcode 0x50 + low3; REX 0x41 if high bit set
    low = REG_CODE[reg] & 0x7
    rex = b'\x41' if (reg.startswith('r') and int(reg[1:])>=8) else b''
    return rex + bytes([0x50 + low])

def emit_mov_reg_reg(dst, src):
    # mov r64, r64: REX.W=1, opcode 0x89, ModR/M with reg=src, rm=dst
    R = 1 if src.startswith('r') and int(src[1:])>=8 else 0
    B = 1 if dst.startswith('r') and int(dst[1:])>=8 else 0
    rex = bytes([0x40 | (1<<3) | (R<<2) | B]) if (R or B or 1) else b''
    modrm = 0xC0 | ((REG_CODE[src]&7)<<3) | (REG_CODE[dst]&7)
    return rex + b'\x89' + bytes([modrm])

def emit_sub_rsp_imm8(value):
    # sub rsp, imm8 uses opcode 0x83 /5 with ModR/M 0xEC (reg=5, rm=4)
    rex = b'\x48'  # REX.W=1
    return rex + b'\x83\xEC' + struct.pack('B', value & 0xFF)

def assemble_prologue():
    out = bytearray()
    out += b'\x55'                    # push rbp
    out += emit_mov_reg_reg('rbp','rsp')
    out += emit_push('rbx')
    out += emit_push('r12')
    out += emit_sub_rsp_imm8(32)
    return bytes(out)

if __name__ == "__main__":
    code = assemble_prologue()
    # write to raw image or memory-mapped region
    with open('prologue.bin','wb') as f:
        f.write(code)