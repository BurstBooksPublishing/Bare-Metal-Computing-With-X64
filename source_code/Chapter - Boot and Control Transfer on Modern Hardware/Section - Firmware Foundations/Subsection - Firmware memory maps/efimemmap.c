#include 
#include 
#include 
#include 

// Populate *map_buffer with the memory map and return map parameters.
// Caller must FreePool(*map_buffer).
EFI_STATUS
GetAndProcessMemoryMap(
    EFI_MEMORY_DESCRIPTOR **map_buffer,
    UINTN *map_size,
    UINTN *map_key,
    UINTN *descriptor_size,
    UINT32 *descriptor_version)
{
    EFI_STATUS Status;
    UINTN Size = 0;
    EFI_MEMORY_DESCRIPTOR *Buffer = NULL;

    // First call to get required size.
    Status = gBS->GetMemoryMap(&Size, Buffer, map_key, descriptor_size, descriptor_version);
    if (Status != EFI_BUFFER_TOO_SMALL) return Status;

    // Allocate pool with a small safety margin.
    Size += 2 * (*descriptor_size);
    Buffer = AllocatePool(Size);
    if (!Buffer) return EFI_OUT_OF_RESOURCES;

    // Actual retrieval.
    Status = gBS->GetMemoryMap(&Size, Buffer, map_key, descriptor_size, descriptor_version);
    if (EFI_ERROR(Status)) { FreePool(Buffer); return Status; }

    // Iterate descriptors and coalesce usable regions.
    UINTN Entries = Size / *descriptor_size;
    EFI_PHYSICAL_ADDRESS cur_base = 0;
    UINT64 cur_size = 0;
    for (UINTN i = 0; i < Entries; ++i) {
        EFI_MEMORY_DESCRIPTOR *d = (EFI_MEMORY_DESCRIPTOR*)((UINT8*)Buffer + i * (*descriptor_size));
        EFI_PHYSICAL_ADDRESS base = d->PhysicalStart;
        UINT64 len = d->NumberOfPages * 4096ULL;
        UINT32 type = d->Type;

        if (type == EfiConventionalMemory) {
            if (cur_size == 0) { cur_base = base; cur_size = len; }
            else if (cur_base + cur_size == base) { cur_size += len; } // contiguous
            else {
                // process previous usable range [cur_base,cur_base+cur_size)
                // e.g., reserve, map identity, or record for loader allocation.
                cur_base = base; cur_size = len;
            }
        } else {
            if (cur_size != 0) {
                // process previous usable range
                cur_size = 0;
            }
            // handle runtime, ACPI, reserved types as needed.
        }
    }
    // Finalize last range if any.

    *map_buffer = Buffer;
    *map_size = Size;
    return EFI_SUCCESS;
}