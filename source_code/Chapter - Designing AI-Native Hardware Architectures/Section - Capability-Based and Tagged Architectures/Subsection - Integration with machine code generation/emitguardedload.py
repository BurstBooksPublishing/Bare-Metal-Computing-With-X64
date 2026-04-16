def gen_guarded_load(dst, addr_reg, tmp_reg, shadow_base_label, trap_label):
    """
    Generate Intel-syntax asm lines for:
      - compute shadow index = addr >> 3
      - load tag = [shadow_base + index]
      - cmp tag, 0
      - jnz trap
      - lfence (optionally)
      - mov dst, [addr]
    Parameters:
      dst, addr_reg, tmp_reg: registers as strings, e.g. "rax","rdi","rcx"
      shadow_base_label: symbol name for shadow table base
      trap_label: label to handle failures
    Returns:
      list of assembly lines (strings)
    """
    asm = []
    # compute index = addr >> 3 ; tmp_reg := addr_reg (preserve addr)
    asm.append(f"mov {tmp_reg}, {addr_reg}        # copy pointer")
    asm.append(f"shr {tmp_reg}, 3                  # index = addr >> 3")
    # load tag: tag in low byte of tmp_reg via addressing
    asm.append(f"movzx rdx, byte ptr [{shadow_base_label} + {tmp_reg}]  # load tag")
    asm.append(f"test rdx, rdx                    # tag == 0 ?")
    asm.append(f"jnz {trap_label}                 # non-zero tag -> trap")
    # serialize before dereference to reduce speculation window
    asm.append("lfence                            # mitigate speculative bypass")
    asm.append(f"mov {dst}, qword ptr [{addr_reg}] # perform guarded load")
    return asm

# Example usage: write lines to file or pass to assembler pipeline.
# gen_guarded_load("rax","rsi","rcx","shadow_base","handle_trap")