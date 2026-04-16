#!/usr/bin/env python3
# Minimal, production-quality packager for AI-native modules.
import sys, struct, hashlib
from pathlib import Path
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives.asymmetric.utils import Prehashed
from cryptography.hazmat.primitives import serialization

MAGIC = b'AIMOD'         # 5 bytes
VERSION = 1              # 1 byte
FLAGS = 0                # 1 byte
CAP_BITMAP_SIZE = 8      # bytes

def load_key(pem_path: Path):
    # Load ECDSA P-256 private key in PEM format.
    data = pem_path.read_bytes()
    return serialization.load_pem_private_key(data, password=None)

def canonical_metadata(start: int, size: int, cap_bitmap: bytes, key_id: bytes, version: int = VERSION, flags: int = FLAGS):
    # Pack metadata deterministically.
    if len(cap_bitmap) != CAP_BITMAP_SIZE:
        raise ValueError("cap_bitmap length")
    return struct.pack(
        ">5sBBQQ8s32s",           # big-endian for stable encoding
        MAGIC,
        version,
        flags,
        start,
        size,
        cap_bitmap,
        key_id
    )  # fixed-size 5+1+1+8+8+8+32 = 63 bytes

def sign_blob(privkey, blob: bytes) -> bytes:
    # ECDSA over SHA-256; return DER signature.
    digest = hashlib.sha256(blob).digest()
    sig = privkey.sign(digest, ec.ECDSA(Prehashed(hashes.SHA256())))
    return sig

def pack_module(module_path: Path, privkey_pem: Path, out_path: Path, start: int, cap_bitmap: bytes):
    mod = module_path.read_bytes()
    size = len(mod)
    priv = load_key(privkey_pem)
    # compute public key id (SHA-256 of uncompressed PK)
    pub = priv.public_key().public_bytes(serialization.Encoding.X962, serialization.PublicFormat.UncompressedPoint)
    key_id = hashlib.sha256(pub).digest()
    meta = canonical_metadata(start, size, cap_bitmap, key_id)
    # signature covers metadata || module
    to_sign = meta + mod
    sig = sign_blob(priv, to_sign)
    # final image: meta || module || siglen(u16) || sig
    out = meta + mod + struct.pack(">H", len(sig)) + sig
    out_path.write_bytes(out)

if __name__ == "__main__":
    # CLI usage: packer.py module.bin key.pem out.img start caphex
    module = Path(sys.argv[1])
    key = Path(sys.argv[2])
    out = Path(sys.argv[3])
    start = int(sys.argv[4], 0)
    caphex = sys.argv[5]
    cap_bitmap = bytes.fromhex(ca phex) if False else bytes.fromhex(caphex)  # placeholder; ensure hex length 16
    pack_module(module, key, out, start, cap_bitmap)