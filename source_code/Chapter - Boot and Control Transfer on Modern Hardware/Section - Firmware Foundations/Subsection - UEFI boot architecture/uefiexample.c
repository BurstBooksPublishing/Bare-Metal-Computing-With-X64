#include 
#include 

/* Entry: called by firmware after image is loaded and relocated. */
EFI_STATUS EFIAPI UefiMain(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    EFI_STATUS Status;
    UINTN MapSize = 0;
    EFI_MEMORY_DESCRIPTOR *MemMap = NULL;
    UINTN MapKey = 0;
    UINTN DescriptorSize = 0;
    UINT32 DescriptorVersion = 0;

    /* Initialize library helpers (EDK2) and print to console (boot services available). */
    InitializeLib(ImageHandle, SystemTable);
    SystemTable->ConOut->OutputString(SystemTable->ConOut, L"UEFI: preparing to hand off to kernel...\r\n");

    /* First call to determine required buffer size; expected to return EFI_BUFFER_TOO_SMALL. */
    Status = SystemTable->BootServices->GetMemoryMap(&MapSize, MemMap, &MapKey, &DescriptorSize, &DescriptorVersion);
    if (Status != EFI_BUFFER_TOO_SMALL) return Status;

    /* Allocate extra space for potential growth during the second retrieval. */
    MapSize += DescriptorSize * 2;
    Status = SystemTable->BootServices->AllocatePool(EfiLoaderData, MapSize, (void**)&MemMap);
    if (EFI_ERROR(Status)) return Status;

    /* Retrieve the memory map. Retry logic may be needed in real boot paths. */
    Status = SystemTable->BootServices->GetMemoryMap(&MapSize, MemMap, &MapKey, &DescriptorSize, &DescriptorVersion);
    if (EFI_ERROR(Status)) {
        SystemTable->BootServices->FreePool(MemMap);
        return Status;
    }

    /* Attempt to exit boot services; if the map changed, repeat the get/exit sequence. */
    Status = SystemTable->BootServices->ExitBootServices(ImageHandle, MapKey);
    if (EFI_ERROR(Status)) {
        /* On failure we must free and retry; real bootloaders loop until success. */
        SystemTable->BootServices->FreePool(MemMap);
        return Status;
    }

    /* Boot services are now unavailable. From here, only runtime services remain. */
    /* Jump to kernel entry (example: physical address 0x100000) by casting and invoking. */
    typedef void (*kernel_entry_t)(void);
    kernel_entry_t kernel = (kernel_entry_t)(UINTN)0x100000;
    kernel();

    /* If kernel returns, halt. */
    for (;;) __asm__ volatile ("hlt");
    return EFI_SUCCESS;
}