/* Minimal deterministic event logging and replay API. */
#include 
#include 
#include 
#include 
#include            /* for CRC32; replace if unavailable */

typedef enum { EVT_TIMER=1, EVT_IRQ=2, EVT_DEV_READ=3 } evt_type_t;

#pragma pack(push,1)
typedef struct {
    uint64_t seq;          /* monotonic sequence number */
    uint64_t tsc;          /* virtual TSC or logical time */
    uint32_t type;         /* evt_type_t */
    uint32_t len;          /* payload length */
    /* payload follows */
} evt_hdr_t;
#pragma pack(pop)

/* Append an event to file. Returns 0 on success. */
int record_event(FILE *f, uint64_t tsc, evt_type_t type,
                 const void *payload, uint32_t len) {
    evt_hdr_t hdr = {0};
    hdr.seq = (uint64_t)ftell(f); /* coarse position use for uniqueness */
    hdr.tsc = tsc;
    hdr.type = (uint32_t)type;
    hdr.len = len;
    if (fwrite(&hdr, sizeof(hdr), 1, f) != 1) return -1;
    if (len && fwrite(payload, len, 1, f) != 1) return -1;
    uint32_t crc = crc32(0L, Z_NULL, 0);
    /* CRC over header and payload */
    crc = crc32(crc, (const unsigned char*)&hdr, sizeof(hdr));
    if (len) crc = crc32(crc, payload, len);
    if (fwrite(&crc, sizeof(crc), 1, f) != 1) return -1;
    fflush(f);
    return 0;
}

/* Replay cursor structure. */
typedef struct { FILE *f; uint64_t read_seq; } replay_ctx_t;

/* Initialize replay context. */
replay_ctx_t *replay_open(const char *path) {
    FILE *f = fopen(path,"rb"); if (!f) return NULL;
    replay_ctx_t *r = malloc(sizeof(*r)); r->f = f; r->read_seq = 0; return r;
}

/* Read next event; caller must free payload. Returns 0 on success. */
int replay_next(replay_ctx_t *r, uint64_t *out_tsc, evt_type_t *out_type,
                void **out_payload, uint32_t *out_len) {
    evt_hdr_t hdr;
    if (fread(&hdr, sizeof(hdr), 1, r->f) != 1) return -1;
    uint32_t len = hdr.len;
    void *payload = len ? malloc(len) : NULL;
    if (len && fread(payload, len, 1, r->f) != 1) { free(payload); return -1; }
    uint32_t crc_read;
    if (fread(&crc_read, sizeof(crc_read),1,r->f) != 1) { free(payload); return -1; }
    uint32_t crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, (const unsigned char*)&hdr, sizeof(hdr));
    if (len) crc = crc32(crc, payload, len);
    if (crc != crc_read) { free(payload); return -2; } /* integrity fail */
    *out_tsc = hdr.tsc; *out_type = (evt_type_t)hdr.type;
    *out_payload = payload; *out_len = len;
    r->read_seq++;
    return 0;
}

/* Close replay. */
void replay_close(replay_ctx_t *r) { fclose(r->f); free(r); }