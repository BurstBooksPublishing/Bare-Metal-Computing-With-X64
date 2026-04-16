#include 
#include 

EFI_STATUS ExitToBootServices(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    EFI_STATUS Status;
    UINTN MemoryMapSize = 0;
    EFI_MEMORY_DESCRIPTOR *MemoryMap = NULL;
    UINTN MapKey;
    UINTN DescriptorSize;
    UINT32 DescriptorVersion;

    // Initial call obtains required size (expect EFI_BUFFER_TOO_SMALL).
    Status = SystemTable->BootServices->GetMemoryMap(&MemoryMapSize,
                                                    MemoryMap,
                                                    &MapKey,
                                                    &DescriptorSize,
                                                    &DescriptorVersion);
    if (Status != EFI_BUFFER_TOO_SMALL) return Status;

    // Allocate with headroom per eq. (1).
    MemoryMapSize += DescriptorSize * 16; // margin for concurrent changes
    Status = SystemTable->BootServices->AllocatePool(EfiLoaderData,
                                                     MemoryMapSize,
                                                     (void**)&MemoryMap);
    if (EFI_ERROR(Status)) return Status;

    // Loop: refresh map, then attempt ExitBootServices. Retry if key changes.
    for (;;) {
        Status = SystemTable->BootServices->GetMemoryMap(&MemoryMapSize,
                                                        MemoryMap,
                                                        &MapKey,
                                                        &DescriptorSize,
                                                        &DescriptorVersion);
        if (EFI_ERROR(Status)) break; // propagate non-retryable error

        Status = SystemTable->BootServices->ExitBootServices(ImageHandle, MapKey);
        if (!EFI_ERROR(Status)) break; // success, boot services terminated

        // EFI_INVALID_PARAMETER typically indicates the memory map changed.
        if (Status == EFI_INVALID_PARAMETER) {
            // Enlarge buffer and retry.
            MemoryMapSize += DescriptorSize * 8;
            SystemTable->BootServices->FreePool(MemoryMap);
            Status = SystemTable->BootServices->AllocatePool(EfiLoaderData,
                                                             MemoryMapSize,
                                                             (void**)&MemoryMap);
            if (EFI_ERROR(Status)) break;
            continue;
        }
        break;
    }

    if (!EFI_ERROR(Status)) SystemTable->BootServices->FreePool(MemoryMap);
    return Status;
}