# instrs: list of tuples ('op', imm) or ('op', reg)
# supported ops: 'push', 'pop', 'sub_rsp', 'add_rsp', 'call', 'ret'
def verify_stack_discipline(instrs, init_rsp=0):
    rsp = init_rsp
    stack_ok = True
    path_checks = []
    for i, ins in enumerate(instrs):
        op, arg = ins
        if op == 'push':
            rsp -= 8
        elif op == 'pop':
            rsp += 8
        elif op == 'sub_rsp':
            rsp -= int(arg)  # arg in bytes
        elif op == 'add_rsp':
            rsp += int(arg)
        elif op == 'call':
            # enforce pre-call alignment: RSP ≡ 0 (mod 16)
            if (rsp % 16) != 0:
                stack_ok = False
                path_checks.append((i, rsp, 'misaligned before call'))
            # call will push return address
            rsp -= 8
        elif op == 'ret':
            # before ret, RSP should equal initial unless nested returns allowed
            if rsp != init_rsp:
                stack_ok = False
                path_checks.append((i, rsp, 'unbalanced at ret'))
            # return conceptually transfers control; keep sim simple
        else:
            raise ValueError('unsupported op '+op)
    return stack_ok, path_checks

# Example usage:
instrs = [
  ('push', None), ('mov', None), # mov ignored by verifier
  ('push', None), ('sub_rsp', 32),
  ('call', 'foo'),
  ('add_rsp', 32), ('pop', None), ('pop', None), ('ret', None)
]
# filter only supported ops for this verifier
filtered = [x for x in instrs if x[0] in ('push','pop','sub_rsp','add_rsp','call','ret')]
print(verify_stack_discipline(filtered, init_rsp=0))