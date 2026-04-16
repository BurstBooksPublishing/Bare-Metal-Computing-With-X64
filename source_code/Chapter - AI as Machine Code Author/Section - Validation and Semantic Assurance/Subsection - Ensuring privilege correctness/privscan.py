from capstone import Cs, CS_ARCH_X86, CS_MODE_64

PRIV_INSNS = {
    "syscall","sysret","sysenter","sysexit","iret","lgdt","lidt","ltr",
    "wrmsr","rdmsr","in","out","cli","sti","hlt","invd","wbinvd"
}

def analyze_bytes(code_bytes, base_addr=0x0):
    md = Cs(CS_ARCH_X86, CS_MODE_64)
    md.detail = False
    findings = []
    for insn in md.disasm(code_bytes, base_addr):
        m = insn.mnemonic.lower()
        op = insn.op_str
        # direct privileged mnemonics
        if m in PRIV_INSNS:
            findings.append((insn.address, m, op, "requires CPL0 or gate/MSR"))
            continue
        # detect MOV to control registers (e.g., mov cr3, rax)
        if m == "mov" and ("cr0" in op or "cr2" in op or "cr3" in op or "cr4" in op):
            findings.append((insn.address, m, op, "control-register access (privileged)"))
            continue
        # detect far control transfers: ljmp/lcall/retf
        if m in ("ljmp","lcall","retf"):
            findings.append((insn.address, m, op, "far control transfer; check selector/CPL semantics"))
            continue
    return findings

# Example usage: read raw .text from an ELF or memory image and call analyze_bytes.
# The tool intentionally leaves MSR/IDT/TSS checks to a secondary verifier that has
# symbol and environment information.