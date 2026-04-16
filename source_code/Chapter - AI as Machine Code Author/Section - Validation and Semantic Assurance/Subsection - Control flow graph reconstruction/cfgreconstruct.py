#!/usr/bin/env python3
# Minimal production-ready CFG builder: uses Capstone and NetworkX.
# Requires: capstone, networkx
from capstone import *
import networkx as nx
from collections import deque

MD = Cs(CS_ARCH_X86, CS_MODE_64)
REG_CONSTS = {'rbx','rax','rcx','rdx','rsi','rdi','rsp','rbp'}

class CFGBuilder:
    def __init__(self, code_bytes, base_addr=0x1000):
        self.code = code_bytes
        self.base = base_addr
        self.g = nx.DiGraph()
        self.blocks = {}
        self.state_in = {}
    def read_bytes(self, addr, size):
        off = addr - self.base
        return self.code[off:off+size]
    def dis_inst(self, addr):
        data = self.read_bytes(addr, 16)
        for i in MD.disasm(data, addr):
            return i
        return None
    def is_direct_branch(self, insn):
        return insn.mnemonic in ('jmp','call') and insn.op_str.startswith('0x')
    def is_cond_branch(self, insn):
        return insn.group(X86_GRP_JUMP) and insn.mnemonic.startswith('j') and insn.mnemonic != 'jmp'
    def build(self, entry):
        work = deque([entry])
        while work:
            addr = work.popleft()
            if addr in self.blocks: continue
            cur = addr
            insns = []
            while True:
                insn = self.dis_inst(cur)
                if insn is None: break
                insns.append(insn)
                cur = insn.address + insn.size
                if insn.mnemonic in ('ret','jmp') or self.is_cond_branch(insn):
                    break
            self.blocks[addr] = insns
            self.g.add_node(addr)
            # Determine outgoing edges
            last = insns[-1] if insns else None
            if last is None: continue
            if last.mnemonic == 'ret':
                self.g.add_edge(addr, 'RET')  # abstract return
            elif self.is_direct_branch(last):
                target = int(last.op_str,16)
                self.g.add_edge(addr, target)
                work.append(target)
            elif self.is_cond_branch(last):
                # taken target
                target = int(last.op_str,16)
                self.g.add_edge(addr, target)
                self.g.add_edge(addr, cur)  # fall-through
                work.append(target); work.append(cur)
            elif last.mnemonic == 'jmp' and last.op_str.startswith('qword ptr'):
                # crude attempt: unresolved indirect
                self.g.add_edge(addr, 'INDIRECT')
            else:
                # fall-through
                self.g.add_edge(addr, cur)
                work.append(cur)
        return self.g

if __name__ == '__main__':
    import sys
    data = open(sys.argv[1],'rb').read()
    builder = CFGBuilder(data, base_addr=0x1000)
    G = builder.build(0x1000)
    print("Nodes:", len(G.nodes()))
    for n in G.nodes(): print(n)