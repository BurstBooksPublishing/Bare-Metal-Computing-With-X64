#!/usr/bin/env python3
# Validate a machine-code blob against architectural constraints.
import sys
from capstone import Cs, CS_ARCH_X86, CS_MODE_64

FORBIDDEN = {
    'hlt','cli','sti','lgdt','lidt','ltr','invlpg','wbinvd','wrmsr','rdmsr',
    'sysenter','sysexit','syscall','sysret'
}

# Conservative per-instruction latency table (cycles). Extend per-CPU.
LATENCY = {
    'mov': 1, 'add': 1, 'sub': 1, 'lea': 2, 'cmp': 1, 'call': 5, 'ret': 1,
    'jmp': 2, 'lock': 10, 'xchg': 3, 'mul': 3, 'div': 20, 'imul':3
}

def estimate_latency(insns):
    total = 0
    for i in insns:
        m = i.mnemonic.split()[0]
        total += LATENCY.get(m, 4)  # default conservative
    return total

def check_forbidden(insns):
    bad = [i for i in insns if i.mnemonic in FORBIDDEN]
    return bad

def check_stack_alignment(entry_rsp):
    # Expect entry_rsp to be provided by environment (symbolic or concrete).
    return (entry_rsp % 16) == 0

def validate(blob, entry_rsp=0x0, deadline_cycles=None):
    md = Cs(CS_ARCH_X86, CS_MODE_64)
    md.detail = False
    insns = list(md.disasm(blob, 0x0))
    bad = check_forbidden(insns)
    latency = estimate_latency(insns)
    alignment_ok = check_stack_alignment(entry_rsp)
    report = {
        'instruction_count': len(insns),
        'forbidden_instructions': [(i.address, i.mnemonic, i.op_str) for i in bad],
        'static_latency_cycles': latency,
        'stack_alignment_ok': alignment_ok
    }
    if deadline_cycles is not None:
        report['meets_deadline'] = latency <= deadline_cycles
    return report

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: validate.py ", file=sys.stderr); sys.exit(2)
    blob = open(sys.argv[1], 'rb').read()
    # Example: entry RSP provided by build system or bootloader environment.
    rpt = validate(blob, entry_rsp=0x7fffffffe000, deadline_cycles=1000)
    for k,v in rpt.items():
        print(f"{k}: {v}")