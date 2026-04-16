from dataclasses import dataclass, field, asdict
from typing import Dict, List, Callable, Any
import json

# Predicate type: takes state dict, returns bool
Predicate = Callable[[Dict[str, Any]], bool]

@dataclass
class MemoryRegion:
    phys_base: int  # physical base address
    size: int       # size in bytes
    attrs: str      # e.g. "R-X", "RW-UC"

@dataclass
class HardwareSpec:
    S0: Dict[str, int]                       # initial registers and flags
    Control: Dict[str, int]                  # CR0, CR3, CR4, IA32_EFER
    MemoryMap: List[MemoryRegion] = field(default_factory=list)
    AllowedInstructions: List[str] = field(default_factory=list)
    Postconditions: List[Predicate] = field(default_factory=list)
    Constraints: List[Predicate] = field(default_factory=list)

    def validate_constraints(self, state: Dict[str, Any]) -> bool:
        # All constraints must hold for given state snapshot
        return all(pred(state) for pred in self.Constraints)

    def check_postconditions(self, final_state: Dict[str, Any]) -> bool:
        return all(pred(final_state) for pred in self.Postconditions)

    def to_json(self) -> str:
        # Serialize MemoryRegion objects cleanly
        d = asdict(self)
        d['MemoryMap'] = [
            {'phys_base': r['phys_base'], 'size': r['size'], 'attrs': r['attrs']}
            for r in d['MemoryMap']
        ]
        return json.dumps(d, indent=2)

# Example predicate helpers
def cr0_pg_enabled(state: Dict[str, Any]) -> bool:
    return bool(state.get('Control', {}).get('CR0', 0) & (1 << 31))

# Example instantiation (values illustrative)
spec = HardwareSpec(
    S0={'RIP': 0x1000, 'RSP': 0x8000},
    Control={'CR0': 0x80050033, 'CR3': 0x0, 'IA32_EFER': 0},
    MemoryMap=[MemoryRegion(0x0, 1<<30, 'RW-UC')],
    AllowedInstructions=['MOV','LGDT','WRMSR','MOV_CR'],
    Postconditions=[cr0_pg_enabled],
    Constraints=[]
)
print(spec.to_json())  # Ready to embed in prompts or verifiers