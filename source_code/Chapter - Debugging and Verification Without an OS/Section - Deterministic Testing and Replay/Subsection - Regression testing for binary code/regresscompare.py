#!/usr/bin/env python3
# Regression comparator: computes SHA256 per-region and Hamming distance.
import sys,hashlib,struct
from pathlib import Path

def sha256_of(path):
    h=hashlib.sha256()
    with open(path,'rb') as f:
        for chunk in iter(lambda: f.read(65536), b''):
            h.update(chunk)
    return h.hexdigest()

def hamming_bytes(a,b):
    # Return number of differing bytes; lengths must match.
    return sum(x!=y for x,y in zip(a,b))

def compare_regions(file_a,file_b,threshold=0):
    a=Path(file_a); b=Path(file_b)
    if a.stat().st_size != b.stat().st_size:
        return False, {"error":"size-mismatch"}
    with open(a,'rb') as fa, open(b,'rb') as fb:
        ma=fa.read(); mb=fb.read()
    sha_a=hashlib.sha256(ma).hexdigest()
    sha_b=hashlib.sha256(mb).hexdigest()
    diff=hamming_bytes(ma,mb)
    ok = diff <= threshold
    return ok, {"sha_a":sha_a,"sha_b":sha_b,"byte_differences":diff}

def load_regs(path):
    # Expect canonical binary: 16 x 8-byte GP registers then RIP and RFLAGS.
    fmt = '<18Q'  # RAX..R15,RIP,RFLAGS
    with open(path,'rb') as f:
        data=f.read(struct.calcsize(fmt))
    return struct.unpack(fmt,data)

def compare_regs(a_path,b_path):
    ra=load_regs(a_path); rb=load_regs(b_path)
    diffs=[i for i,(x,y) in enumerate(zip(ra,rb)) if x!=y]
    return diffs

if __name__=='__main__':
    if len(sys.argv)<5:
        print("Usage: regress_compare.py memA memB regsA regsB [threshold]")
        sys.exit(2)
    memA,memB,regsA,regsB=sys.argv[1:5]
    thresh=int(sys.argv[5]) if len(sys.argv)>5 else 0
    ok,info=compare_regions(memA,memB,thresh)
    print("Memory SHA A:", info.get("sha_a"))
    print("Memory SHA B:", info.get("sha_b"))
    print("Byte differences:", info.get("byte_differences"))
    reg_diffs=compare_regs(regsA,regsB)
    print("Register differences at indices:", reg_diffs)
    sys.exit(0 if ok and not reg_diffs else 1)