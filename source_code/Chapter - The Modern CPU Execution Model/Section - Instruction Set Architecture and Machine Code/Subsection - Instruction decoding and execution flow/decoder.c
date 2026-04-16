#include 
#include 
#include 

// Result structure returned to caller.
typedef struct {
    size_t length;         // total instruction bytes consumed
    uint8_t rex;           // REX prefix if present, else 0
    uint8_t opcode0;       // first opcode byte
    bool has_modrm;
    uint8_t modrm;         // ModR/M byte if present
    bool has_sib;
    uint8_t sib;           // SIB byte if present
    int32_t disp;          // sign-extended displacement (0 if none)
    int64_t imm;           // sign-extended immediate (0 if none)
} insn_t;

// Parse bytes safely; return false on malformed or exceeding bounds.
bool decode_insn(const uint8_t *buf, size_t buflen, insn_t *out) {
    const uint8_t *p = buf;
    const uint8_t *end = buf + buflen;
    *out = (insn_t){0};
    // 1) Scan legacy prefixes (0xF0..0xF3, 0x66, 0x67, segment overrides)
    while (p < end) {
        uint8_t b = *p;
        if (b==0xF0 || (b>=0xF2 && b<=0xF3) || b==0x66 || b==0x67 ||
            (b>=0x26 && b<=0x3E && (b&0x0E)==0x00)) { p++; continue; }
        break;
    }
    // 2) Optional REX in 64-bit mode: 0x40..0x4F
    if (p < end && (*p & 0xF0) == 0x40) { out->rex = *p++; }
    // 3) Opcode byte(s)
    if (p >= end) return false;
    out->opcode0 = *p++;
    // handle two-byte escape 0x0F
    if (out->opcode0 == 0x0F) {
        if (p >= end) return false;
        out->opcode0 = 0x0F; // mark presence; further decoding needed in full decoder
        // for brevity, treat second opcode as part of opcode stream; don't parse here
        // advance second opcode byte (safe check)
        p++; if (p > end) return false;
    }
    // 4) Optional ModR/M
    // Rough heuristic: most primary opcodes require ModR/M, but a full table is needed.
    // Here, detect presence by opcode ranges that commonly require ModR/M.
    uint8_t op = out->opcode0;
    bool opcode_requires_modrm = (op & 0xC0) != 0xC0; // conservative fallback
    if (opcode_requires_modrm && p < end) {
        out->has_modrm = true;
        out->modrm = *p++;
        uint8_t mod = (out->modrm >> 6) & 3;
        uint8_t rm  = out->modrm & 7;
        if (mod != 3 && rm == 4) { // SIB present
            if (p >= end) return false;
            out->has_sib = true;
            out->sib = *p++;
        }
        // displacement sizes
        if (mod == 1) { // 8-bit disp
            if (p >= end) return false;
            out->disp = (int8_t)*p++; // sign-extend
        } else if (mod == 2) { // 32-bit disp
            if (end - p < 4) return false;
            int32_t d = (int32_t)(p[0] | (p[1]<<8) | (p[2]<<16) | (p[3]<<24));
            out->disp = d; p += 4;
        } else if (mod == 0 && rm == 5) { // disp32 without base
            if (end - p < 4) return false;
            int32_t d = (int32_t)(p[0] | (p[1]<<8) | (p[2]<<16) | (p[3]<<24));
            out->disp = d; p += 4;
        }
    }
    // 5) Immediate detection: conservatively check common immediate opcodes (ADD imm, etc.)
    // For correctness, consult opcode map; here we parse a trailing 32-bit immediate if bytes remain.
    if (end - p >= 4) {
        // This simplistic heuristic will treat trailing 4 bytes as immediate.
        int32_t imm32 = (int32_t)(p[0] | (p[1]<<8) | (p[2]<<16) | (p[3]<<24));
        out->imm = imm32;
        p += 4;
    }
    out->length = (size_t)(p - buf);
    if (out->length > 15) return false; // respect architectural limit
    return true;
}