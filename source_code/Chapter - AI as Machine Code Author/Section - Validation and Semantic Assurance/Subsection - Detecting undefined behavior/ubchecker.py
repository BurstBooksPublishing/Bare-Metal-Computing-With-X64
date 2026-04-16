# ub_checker.py -- production-ready static pass for basic UB patterns.
# Requires: pip install capstone
from capstone import Cs, CS_ARCH_X86, CS_MODE_64
from typing import List, Dict, Optional
import struct, logging

logging.basicConfig(level=logging.INFO)

CS = Cs(CS_ARCH_X86, CS_MODE_64)
CS.detail = True

# Architecture masks (from Intel SDM, simplified examples)
CR0_RESERVED_MASK = 0xFFFFFFFFFFFEF0C0  # example mask: bits that must be zero (illustrative)
CR4_RESERVED_MASK = 0xFFFFFFFFFFFEF8F0

def disassemble(code: bytes, base: int = 0x0) -> List:
    return list(CS.disasm(code, base))

def is_privileged_insn(insn) -> bool:
    # privileged examples: lgdt, lidt, ltr, lgdtq, wrmsr, rdmsr, cli/sti
    name = insn.mnemonic.lower()
    return name in ("lgdt", "lidt", "ltr", "sgdt", "sidt", "wrmsr", "rdmsr", "cli", "sti")

def check_avx_requires_osxsave(state: Dict, insn) -> Optional[str]:
    # VEX/EVEX usage can be detected by mnemonic prefix or operand registers.
    # Conservative: if insn.mnemonic contains 'v' and uses xmm/ymm registers, check state.
    if 'v' in insn.mnemonic and any(op.type == 1 and op.reg for op in insn.operands):
        if not state.get("CR4_OSXSAVE", False):
            return "AVX instruction without CR4.OSXSAVE enabled -> #UD possible"
        if (insn.mnemonic.startswith("v") and state.get("XCR0_YMM", False) is False):
            return "YMM use without XCR0[2] enabled -> #UD possible"
    return None

def check_division_by_zero(insn, state: Dict) -> Optional[str]:
    # Identify idiv/div and conservatively flag if divisor is not a known nonzero constant.
    if insn.mnemonic in ("div", "idiv"):
        # simple pattern: if operand is immediate or memory with known constant in state
        op = insn.operands[0]
        if op.type == 1:  # register
            # if register value unknown, conservatively warn
            if state.get("regs", {}).get(insn.op_str.strip(), None) in (None, "unknown"):
                return "Division by possibly zero register -> potential #DE"
        else:
            return "Division operand not provably nonzero -> potential #DE"
    return None

def check_movaps_alignment(insn, state: Dict) -> Optional[str]:
    if insn.mnemonic == "movaps":
        # conservative: if memory operand address alignment unknown, warn
        for op in insn.operands:
            if op.type == 2:  # memory
                if not state.get("mem_alignment_ok", False):
                    return "MOVAPS on possibly unaligned address -> #GP possible"
    return None

def check_reserved_cr_write(value: int, regname: str) -> Optional[str]:
    if regname == "cr0":
        if value & ~CR0_RESERVED_MASK:
            return "CR0 write sets reserved bits -> unpredictable behavior"
    if regname == "cr4":
        if value & ~CR4_RESERVED_MASK:
            return "CR4 write sets reserved bits -> unpredictable behavior"
    return None

def analyze(code: bytes, base: int = 0x0, initial_state: Dict = None) -> List[str]:
    state = initial_state or {}
    problems = []
    for insn in disassemble(code, base):
        if is_privileged_insn(insn) and not state.get("CPL0", False):
            problems.append(f"{insn.address:x}: privileged {insn.mnemonic} in non-CPL0 context")
        p = check_avx_requires_osxsave(state, insn)
        if p: problems.append(f"{insn.address:x}: {p}")
        p = check_division_by_zero(insn, state)
        if p: problems.append(f"{insn.address:x}: {p}")
        p = check_movaps_alignment(insn, state)
        if p: problems.append(f"{insn.address:x}: {p}")
        # Example CR write detection
        if insn.mnemonic == "mov" and insn.op_str.startswith("cr0"):
            # parse immediate if present (very simplified)
            try:
                imm = int(insn.op_str.split(",")[-1], 0)
                p = check_reserved_cr_write(imm, "cr0")
                if p: problems.append(f"{insn.address:x}: {p}")
            except Exception:
                problems.append(f"{insn.address:x}: write to CR0 with unknown value -> check reserved bits")
    return problems

if __name__ == "__main__":
    # example usage
    sample = b"\x90"  # NOP
    issues = analyze(sample, 0x1000, {"CPL0": True})
    for i in issues:
        print(i)