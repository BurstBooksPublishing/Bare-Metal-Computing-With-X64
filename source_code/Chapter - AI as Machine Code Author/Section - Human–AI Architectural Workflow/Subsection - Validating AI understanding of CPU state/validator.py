#!/usr/bin/env python3
# Minimal, production-ready validator: launches gdb, reads registers, compares to expected JSON.
import subprocess, json, re, sys
HEX_RE = re.compile(r'0x[0-9a-fA-F]+')
def run_gdb_cmds(port, cmds):
    # Run gdb with remote target at localhost:port and execute cmds; return stdout.
    args = ['gdb', '-nx', '-ex', f'target remote :{port}'] + [f'-ex {c}' for c in cmds] + ['-ex', 'quit', '--batch']
    out = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True, text=True)
    return out.stdout
def parse_info_registers(gdb_out):
    regs = {}
    for line in gdb_out.splitlines():
        parts = line.strip().split()
        if len(parts) >= 2 and HEX_RE.match(parts[1]):
            regs[parts[0]] = int(parts[1], 16)
    return regs
def load_expected(path):
    with open(path,'r') as f:
        return {k:int(v,16) for k,v in json.load(f).items()}
def hamming64(a,b): return bin(a ^ b).count('1')
def compute_delta(pred, obs, weights):
    num = sum(weights.get(r,1)*hamming64(pred[r], obs.get(r,0)) for r in pred)
    den = sum(weights.get(r,1)*64 for r in pred)
    return num/den
if __name__ == '__main__':
    if len(sys.argv)!=3: 
        print("usage: validator.py  ", file=sys.stderr); sys.exit(2)
    port,expect_path = sys.argv[1], sys.argv[2]
    gdb_out = run_gdb_cmds(port, ['info registers'])
    observed = parse_info_registers(gdb_out)
    expected = load_expected(expect_path)
    weights = {r: (10 if r in ('rip','rsp') else 1) for r in expected}
    delta = compute_delta(expected, observed, weights)
    print(f'delta={delta:.6f}')
    if delta==0.0:
        print('VALIDATION: PASS')
        sys.exit(0)
    else:
        # Emit compact diff for refinement loop
        for r in expected:
            e=expected[r]; o=observed.get(r,0)
            if e!=o:
                print(f'{r}: expected=0x{e:016x} observed=0x{o:016x} diffbits={hamming64(e,o)}')
        sys.exit(1)