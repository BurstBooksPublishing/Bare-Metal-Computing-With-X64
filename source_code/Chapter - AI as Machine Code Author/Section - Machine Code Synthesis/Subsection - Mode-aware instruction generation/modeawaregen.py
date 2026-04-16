import struct

# mode in {'real16','protected32','long64'}
def generate_mov_reg_imm(mode: str, reg: int, imm: int) -> bytes:
    # validate register
    if not (0 <= reg <= 15):
        raise ValueError("reg must be 0..15")
    # default operand size D
    if mode == 'real16':
        D = 16
    elif mode in ('protected32',):
        D = 32
    elif mode == 'long64':
        D = 32  # legacy default in long mode
    else:
        raise ValueError("unsupported mode")

    # determine whether we need 64-bit (only allowed in long mode)
    need64 = isinstance(imm, int) and (-2**63 <= imm < 2**64) and (imm.bit_length() > 32) 
    # simple heuristic: if imm needs >32 bits then request 64-bit
    if need64 and mode != 'long64':
        raise ValueError("64-bit immediates require long64 mode")

    # decide W and prefixes
    rex = None
    prefix_bytes = b''
    if mode == 'long64' and need64:
        W = 64
        # compose REX: 0x40 | (W<<3) | B where B = high bit of reg
        rex_w = 1
        rex_b = (reg >> 3) & 1
        rex = bytes([0x40 | (rex_w << 3) | rex_b])
    else:
        # no REX; assume W = D (no operand-size prefix used here)
        W = D

    # check register availability in real16
    if mode == 'real16' and reg >= 8:
        raise ValueError("registers R8-R15 unavailable in real16")

    # opcode: MOV r, imm uses 0xB8 + low3(reg)
    opcode = bytes([0xB8 + (reg & 0x7)])

    # immediate encoding
    imm_size = W // 8
    fmt = {1:'<B',2:'<H',4:'<I',8:'<Q'}[imm_size]
    if imm.bit_length() > (imm_size*8):
        raise ValueError("immediate too large for selected width")
    imm_bytes = struct.pack(fmt, imm)

    # assemble final bytes (prefixes, REX only valid in long mode)
    parts = []
    if prefix_bytes:
        parts.append(prefix_bytes)
    if rex:
        parts.append(rex)
    parts.append(opcode)
    parts.append(imm_bytes)
    return b''.join(parts)

# Example usage:
# print(generate_mov_reg_imm('long64', 0, 0x1122334455667788).hex())