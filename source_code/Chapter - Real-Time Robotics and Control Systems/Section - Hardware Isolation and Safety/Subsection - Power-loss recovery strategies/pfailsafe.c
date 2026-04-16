int powerfail_persist(const void *payload, size_t bytes)
{
    const size_t max_bytes = 4096;                  /* HW limit */
    size_t write_bytes = (bytes > max_bytes) ? max_bytes : bytes;

    disable_interrupts();                           /* deterministic path */
    engage_mechanical_brake();                      /* safe-stop hook */

    journal_header_t hdr = { .magic = HDR_MAGIC, .len = write_bytes,
                             .version = next_version() };
    hdr.crc = crc32(&hdr, sizeof(hdr) - sizeof(hdr.crc));

    /* write header then payload; flash_write is power-fail safe block op */
    if (flash_write(JOURNAL_HDR_ADDR, &hdr, sizeof(hdr)) != 0) goto fail;
    if (flash_write(JOURNAL_PAYLOAD_ADDR, payload, write_bytes) != 0) goto fail;

    /* commit marker: single-word atomic flag with checksum */
    uint32_t marker = compute_marker(hdr.version, write_bytes);
    if (flash_write(JOURNAL_MARKER_ADDR, &marker, sizeof(marker)) != 0) goto fail;

    /* optional verify read-back for safety */
    if (flash_read_verify(JOURNAL_MARKER_ADDR, &marker, sizeof(marker)) != 0) goto fail;

    enable_interrupts();
    return 0;

fail:
    /* best-effort fallback: write minimal sanity info to FRAM, keep brakes engaged */
    fram_write(SANITY_ADDR, &sanity_blob, sizeof(sanity_blob));
    enable_interrupts();
    return -2;
}