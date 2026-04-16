from z3 import Solver, Bool, Int, And, Or, sat, simplify

# Simple model: CR3 holds pml4_base and pcid (low 12 bits)
pml4_old, pcid_old = Int('pml4_old'), Int('pcid_old')
pml4_new, pcid_new = Int('pml4_new'), Int('pcid_new')
# Mapping validity predicate (1 = valid, 0 = invalid)
mapping_preserved = Bool('mapping_preserved')

s = Solver()
# Model assumption: initial model assumed full flush on CR3 write
# invariant: after cr3 write, no preserved mappings must exist
s.add( mapping_preserved == False )  # invariant according to M0

# But a realistic observation: preserved mapping can exist when pcid unchanged
# Encode possibility: if pcid unchanged then mapping_preserved can be True
s.push()
s.add(pcid_old == pcid_new, mapping_preserved == True)
if s.check() == sat:
    m = s.model()
    # produce a counterexample for human inspection
    ce = {
        'pcid_old': m[pcid_old].as_long(),
        'pcid_new': m[pcid_new].as_long(),
        'note': 'PCID unchanged allows preserved mappings; refine TLB semantics.'
    }
    print(ce)
s.pop()

# Record refinement action (pseudo-persistent)
refinement = {
    'add': 'Include PCID-aware TLB semantics: conditional preservation when PCID unchanged',
    'affected': ['CR3_write', 'TLB_invalidate']
}
print('Refinement suggested:', refinement)