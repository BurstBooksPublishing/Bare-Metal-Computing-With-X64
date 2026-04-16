#include 
#include 

// Parameters: caller provides physically-contiguous, page-aligned buffers.
// This routine only builds structures in memory; the caller must write
// the physical addresses into the IOMMU registers discovered via ACPI DMAR.
#define PAGE_SIZE 4096
#define ROOT_ENTRIES 256

// Basic flag values per Intel VT-d (present bit = 1). Adjust per HW ECAP.
#define ENTRY_PRESENT 0x1ULL
#define PTE_RW         (1ULL<<1)   // example RW flag bit for device (implementation defined)

void build_iommu_structures(void *root_buf,
                            void *ctx_buf_for_bus,     // one 4K context table
                            void *pt_leaf_buf,         // leaf page-table page (4K)
                            uint8_t bus, uint8_t dev, uint8_t func,
                            uint64_t phys_map_base,    // physical buffer to expose
                            uint64_t iova_base,        // IOVA base visible to device
                            size_t map_bytes) {
    // Zero the tables
    memset(root_buf, 0, PAGE_SIZE);
    memset(ctx_buf_for_bus, 0, PAGE_SIZE);
    memset(pt_leaf_buf,  0, PAGE_SIZE);

    // Compute indices: context index uses device:function encoded as (dev<<3)|func
    uint8_t ctx_index = (dev << 3) | (func & 0x7);

    // Root table entry: point to context table physical address (caller-supplied)
    // The caller must convert the virtual pointers here to physical addresses
    // appropriate for the IOMMU. For this in-memory build we store placeholders.
    uint64_t ctx_phys = (uint64_t)ctx_buf_for_bus; // caller must replace with phys
    uint64_t root_entry = (ctx_phys & ~0xFFFULL) | ENTRY_PRESENT; // lower bits are flags
    ((uint64_t*)root_buf)[bus] = root_entry;

    // Context entry: point to page-table root (here single leaf level).
    uint64_t pt_phys = (uint64_t)pt_leaf_buf; // must be physical in real setup
    uint64_t ctx_entry = (pt_phys & ~0xFFFULL) | ENTRY_PRESENT;
    ((uint64_t*)ctx_buf_for_bus)[ctx_index] = ctx_entry;

    // Build simple 4 KiB PTEs mapping sequential pages
    size_t pages = (map_bytes + PAGE_SIZE - 1) / PAGE_SIZE;
    for (size_t i = 0; i < pages; ++i) {
        uint64_t pte = ((phys_map_base + i*PAGE_SIZE) & ~0xFFFULL) | PTE_RW | ENTRY_PRESENT;
        ((uint64_t*)pt_leaf_buf)[ (i + ( (iova_base / PAGE_SIZE) & (PAGE_SIZE/8-1) )) ] = pte;
    }
    // Note: real hardware requires full page-table hierarchy and correct index computation.
}