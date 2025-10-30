#!/usr/bin/env python3
"""
Mine two Bells testnet fork headers (height 1 & 2) off a supplied parent hash.

Usage example:
    python3 contrib/devtools/make_fork_headers.py \
        --prev  e5be24df57c43a82d15c2f06bda961296948f8f8eb48501bed1efb929afe0698 \
        --bits  1e0ffff0 \
        --time  1383509530

The script prints two `fork:` lines you can paste into
test/functional/data/blockheader_bells_testnet.hex and a final TIP=... line
for the test assertions.
"""
import argparse
import hashlib
import struct
from dataclasses import dataclass

import ltc_scrypt

def bits_to_target(bits_hex: str) -> int:
    bits = int(bits_hex, 16)
    exponent = bits >> 24
    mantissa = bits & 0x00FFFFFF
    if mantissa == 0 or exponent < 3:
        raise ValueError(f"Bad nBits value: {bits_hex}")
    return mantissa << (8 * (exponent - 3))

def little_endian(hexstr: str) -> bytes:
    data = bytes.fromhex(hexstr)
    if len(data) != 32:
        raise ValueError(f"Expected 32-byte hex, got len={len(data)}: {hexstr}")
    return data[::-1]

def double_sha256(data: bytes) -> bytes:
    return hashlib.sha256(hashlib.sha256(data).digest()).digest()

def pow_scrypt(header: bytes) -> bytes:
    # Bells uses Litecoin-style scrypt(N=1024, r=1, p=1, 32-byte digest)
    return ltc_scrypt.getPoWHash(header)

@dataclass
class MinedHeader:
    header_hex: str
    block_hash: str
    nonce: int

def mine_header(version: int, prev_hash_hex: str, merkle_hex: str,
                timestamp: int, bits_hex: str, target: int) -> MinedHeader:
    prev_le = little_endian(prev_hash_hex)
    merkle_le = little_endian(merkle_hex)
    bits_int = int(bits_hex, 16)

    for nonce in range(0, 2**32):
        header = (
            struct.pack("<I", version) +
            prev_le +
            merkle_le +
            struct.pack("<I", timestamp) +
            struct.pack("<I", bits_int) +
            struct.pack("<I", nonce)
        )
        work_hash = pow_scrypt(header)
        work_value = int.from_bytes(work_hash[::-1], "big")
        if work_value <= target:
            block_hash = double_sha256(header)[::-1].hex()
            return MinedHeader(header.hex(), block_hash, nonce)
    raise RuntimeError("Exhausted nonce space without finding valid header")

def deterministic_merkle(height: int) -> str:
    return hashlib.sha256(f"bells-fork-height-{height}".encode()).hexdigest()

def main() -> None:
    parser = argparse.ArgumentParser(description="Mine two Bells fork headers")
    parser.add_argument("--prev", required=True,
                        help="Hex block hash to fork from (genesis hash)")
    parser.add_argument("--bits", required=True,
                        help="Compact target (nBits) for heights 1 and 2, hex")
    parser.add_argument("--time", required=True, type=int,
                        help="Parent block unix timestamp")
    parser.add_argument("--version", default="1",
                        help="Block version (default: 1 / 0x00000001)")
    parser.add_argument("--time-step", type=int, default=5,
                        help="Seconds to add per new header (default: 5)")
    args = parser.parse_args()

    version = int(args.version, 0)
    target = bits_to_target(args.bits)

    # Height 1
    ts1 = args.time + args.time_step
    h1 = mine_header(
        version=version,
        prev_hash_hex=args.prev,
        merkle_hex=deterministic_merkle(1),
        timestamp=ts1,
        bits_hex=args.bits,
        target=target,
    )
    print(f"fork:{h1.header_hex}")

    # Height 2
    ts2 = ts1 + args.time_step
    h2 = mine_header(
        version=version,
        prev_hash_hex=h1.block_hash,
        merkle_hex=deterministic_merkle(2),
        timestamp=ts2,
        bits_hex=args.bits,
        target=target,
    )
    print(f"fork:{h2.header_hex}")
    print(f"TIP={h2.block_hash}")

if __name__ == "__main__":
    main()
