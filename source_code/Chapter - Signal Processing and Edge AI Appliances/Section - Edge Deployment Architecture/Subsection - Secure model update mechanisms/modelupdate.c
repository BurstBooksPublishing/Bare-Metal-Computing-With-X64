#include "mbedtls/sha256.h"
#include "mbedtls/ecdsa.h"
#include "flash_hw.h"     // platform flash primitives: flash_write/read/erase
#include "tpm_iface.h"    // platform TPM: monotonic counter read/increment

// Verify signature using mbed TLS ECDSA; returns 0 on success.
int verify_manifest_signature(const uint8_t *msg, size_t msglen,
                              const uint8_t *sig, size_t siglen,
                              const mbedtls_ecp_point *pubQ, mbedtls_ecp_group *grp)
{
    mbedtls_sha256_context sha;
    uint8_t hash[32];
    mbedtls_sha256_init(&sha);
    mbedtls_sha256_starts_ret(&sha, 0);
    mbedtls_sha256_update_ret(&sha, msg, msglen);
    mbedtls_sha256_finish_ret(&sha, hash);
    mbedtls_sha256_free(&sha);

    mbedtls_mpi r, s;
    mbedtls_mpi_init(&r); mbedtls_mpi_init(&s);
    // parse signature (ASN.1 DER) into r,s
    int ret = mbedtls_ecdsa_read_signature_der(grp, &r, &s, sig, siglen);
    if (ret) goto out;

    // Verify: compute u1,u2,R as per Eq. (1); mbedtls_ecdsa_verify does this internally.
    ret = mbedtls_ecdsa_verify(&grp->G, hash, sizeof(hash), pubQ, &r, &s);

out:
    mbedtls_mpi_free(&r); mbedtls_mpi_free(&s);
    return ret; // 0 == OK
}

// High-level update routine (blocking)
int perform_model_update(const uint8_t *model_buf, size_t model_len,
                         const uint8_t *manifest, size_t manifest_len,
                         const uint8_t *sig, size_t siglen,
                         const mbedtls_ecp_point *pubQ, mbedtls_ecp_group *grp)
{
    uint64_t hw_counter = tpm_read_monotonic();               // protected read
    uint64_t manifest_counter = parse_manifest_counter(manifest);

    if (manifest_counter <= hw_counter) return -1;            // anti-rollback

    if (verify_manifest_signature(manifest, manifest_len, sig, siglen, pubQ, grp)) return -2;

    int inactive_slot = select_inactive_slot();               // choose slot B if A active
    if (flash_write_slot(inactive_slot, model_buf, model_len)) return -3;
    if (!flash_verify_slot_hash(inactive_slot, model_buf, model_len)) return -4;

    uint64_t meta_old = read_metadata_atomic();               // read current meta
    uint64_t meta_new = build_metadata_for_slot(inactive_slot, manifest_version(manifest));
    // atomic CAS and persist; if concurrent change, fail safely
    if (!__atomic_compare_exchange_n(&metadata_word, &meta_old, meta_new, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
        // cleanup; do not modify hw_counter to preserve monotonicity
        return -5;
    }
    // persist metadata to flash and then advance hardware counter
    if (flash_persist_metadata(meta_new)) {
        tpm_increment_monotonic();
        return 0;
    }
    // on persist failure, attempt rollback by restoring old metadata word
    __atomic_store_n(&metadata_word, meta_old, __ATOMIC_SEQ_CST);
    return -6;
}