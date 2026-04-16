#include 
#include 
#include 

// Public descriptor returned by decoder.
typedef struct {
    uint8_t opcode_primary;    // first opcode byte
    uint8_t opcode_secondary;  // 0 if none
    uint8_t rex;               // 0 if none, otherwise 0x40-0x4F
    uint8_t modrm;             // 0 if not present
    uint8_t sib;               // 0 if not present
    uint64_t imm;              // up to 64-bit immediate
    size_t length;             // total bytes consumed
    bool valid;
} instr_desc_t;

// Read safe byte from buffer or return 0 and mark invalid.
static inline bool read_byte(const uint8_t *buf, size_t bufsz, size_t off, uint8_t *out) {
    if (off >= bufsz) return false;
    *out = buf[off];
    return true;
}

// Decode a single instruction; buf must point to instruction stream.
instr_desc_t decode_instr(const uint8_t *buf, size_t bufsz) {
    instr_desc_t d = {0};
    size_t off = 0;
    uint8_t b;

    // Prefix scan: accept up to 4 legacy prefixes (for example).
    for (int i = 0; i < 4; ++i) {
        if (!read_byte(buf, bufsz, off, &b)) { d.valid = false; return d; }
        // Example set of legacy prefixes (grouped); break on non-prefix.
        if ((b == 0x66) || (b == 0x67) || ((b & 0xF0) == 0xF0)) { ++off; continue; }
        break;
    }

    // Optional REX detection
    if (!read_byte(buf, bufsz, off, &b)) { d.valid = false; return d; }
    if ((b & 0xF0) == 0x40) { d.rex = b; ++off; if (!read_byte(buf, bufsz, off, &b)) { d.valid = false; return d; } }

    // Primary opcode
    d.opcode_primary = b; ++off;

    // Detect two-byte escape (custom extension); here we use 0x0F as canonical escape.
    if (d.opcode_primary == 0x0F) {
        if (!read_byte(buf, bufsz, off, &b)) { d.valid = false; return d; }
        d.opcode_secondary = b; ++off;
    }

    // Decide if ModR/M is required via a small static predicate table (omitted here).
    bool needs_modrm = true; // in practice consult opcode map
    if (needs_modrm) {
        if (!read_byte(buf, bufsz, off, &b)) { d.valid = false; return d; }
        d.modrm = b; ++off;
        uint8_t mod = (b >> 6) & 0x3, rm = b & 0x7;
        if ((mod != 3) && (rm == 4)) { // SIB present
            if (!read_byte(buf, bufsz, off, &b)) { d.valid = false; return d; }
            d.sib = b; ++off;
        }
        // Displacement handling (simplified)
        if ((mod == 1)) { // 8-bit disp
            uint8_t disp8;
            if (!read_byte(buf, bufsz, off, &disp8)) { d.valid = false; return d; }
            d.length += 0; // imm field unused here; real model records displacement
            ++off;
        } else if (mod == 2) { // 32-bit disp
            uint32_t disp32 = 0;
            for (int i = 0; i < 4; ++i) { if (!read_byte(buf, bufsz, off, &b)) { d.valid = false; return d; } disp32 |= ((uint32_t)b) << (8*i); ++off; }
            (void)disp32;
        }
    }

    // Immediate handling: determine by opcode map (simplified example: opcodes 0xC7 or 0x81)
    if ((d.opcode_primary == 0xC7) || (d.opcode_primary == 0x81)) {
        uint32_t imm32 = 0;
        for (int i = 0; i < 4; ++i) { if (!read_byte(buf, bufsz, off, &b)) { d.valid = false; return d; } imm32 |= ((uint32_t)b) << (8*i); ++off; }
        d.imm = imm32;
    }

    d.length = off;
    d.valid = true;
    return d;
}