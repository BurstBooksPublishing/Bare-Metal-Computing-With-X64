# Minimal encoder: mov dest_reg, src_reg  (64-bit registers 0..15)
# dest, src are integers 0..15 (RAX=0, RCX=1, RDX=2, RBX=3, RSP=4, RBP=5, RSI=6, RDI=7, R8=8,...)
def encode_mov_reg_reg(dest, src):
    # opcode: 0x89 /r moves r64 (reg) -> r/m64 (rm)
    opcode = 0x89
    # REX: 0b0100WRXB : 0x40 | (W<<3) | (R<<2) | (X<<1) | B
    W = 1  # 64-bit operand
    R = (src >> 3) & 1
    X = 0
    B = (dest >> 3) & 1
    rex = 0x40 | (W << 3) | (R << 2) | (X << 1) | B
    # ModR/M: mod=11 (register-direct) => 0b11 reg(3) rm(3)
    mod = 0b11
    reg_field = src & 0x7
    rm_field = dest & 0x7
    modrm = (mod << 6) | (reg_field << 3) | rm_field
    return bytes((rex, opcode, modrm))

# Example: mov rax, rbx  (dest=RAX=0, src=RBX=3)
if __name__ == "__main__":
    seq = encode_mov_reg_reg(0, 3)
    print(seq.hex())  # prints "4889d8"