#!/usr/bin/env python3
# Copyright (c) 2023 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test validateaddress for Bells main chain (bel HRP)

This test regenerates valid and invalid Bech32(m) addresses using the Bells
mainnet HRP ("bel") and exercises the same categories covered by the Bitcoin
Core test, but adapted to this network.
"""

from test_framework.test_framework import BellscoinTestFramework

from test_framework.util import assert_equal
import itertools

from test_framework.segwit_addr import (
    encode_segwit_address,
    bech32_encode,
    convertbits,
    Encoding,
)


HRP = "bel"  # Bells mainnet HRP


def spk_from_witness(ver: int, prog: bytes) -> str:
    if ver == 0:
        return (b"\x00" + bytes([len(prog)]) + prog).hex()
    assert 1 <= ver <= 16
    return (bytes([0x50 + ver]) + bytes([len(prog)]) + prog).hex()


def valid_addr(ver: int, prog_hex: str) -> tuple[str, str]:
    prog = bytes.fromhex(prog_hex)
    addr = encode_segwit_address(HRP, ver, list(prog))
    return addr, spk_from_witness(ver, prog)

def encode_wrong_encoding(ver: int, prog_hex: str, use_bech32: bool) -> str:
    prog = bytes.fromhex(prog_hex)
    data = [ver] + convertbits(list(prog), 8, 5)
    return bech32_encode(Encoding.BECH32 if use_bech32 else Encoding.BECH32M, HRP, data)


def with_invalid_char(addr: str, index_from_sep: int) -> tuple[str, int]:
    # Replace a data character by an invalid base32 char 'b'
    pos = addr.find('1') + 1 + index_from_sep
    return addr[:pos] + 'b' + addr[pos + 1 :], pos


def with_mixed_case(addr: str, index_from_sep: int) -> tuple[str, int]:
    pos = addr.find('1') + 1 + index_from_sep
    ch = addr[pos]
    return addr[:pos] + ch.upper() + addr[pos + 1 :], pos


def empty_data_bech32() -> str:
    return bech32_encode(Encoding.BECH32, HRP, [])


def make_test_vectors():
    invalid = []
    valid = []

    # Some canonical valid witness programs
    v0_20 = "751e76e8199196d454941c45d1b3a323f1433bd6"
    v0_32 = bytes(32).hex()
    v1_32 = "79be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798"
    v1_02 = "4e73"  # P2A sample

    # Valid addresses
    valid.append(valid_addr(0, v0_20))
    valid.append(valid_addr(0, v0_32))
    valid.append(valid_addr(1, v1_32))
    valid.append(valid_addr(1, v1_02))

    # Invalid checksum (deterministic): change first data char 'q' -> 'p'
    base = valid[0][0]
    sep = base.find('1')
    assert sep != -1 and base[sep+1] == 'q'
    addr_bad = base[:sep+1] + 'p' + base[sep+2:]
    invalid.append((addr_bad, "Invalid Bech32 checksum", [sep+1]))

    # Invalid HRP (other networks)
    invalid.append(("tc1qw508d6qejxtdg4y5r3zarvary0c5xw7kg3g4ty",
                    "Invalid or unsupported Segwit (Bech32) or Base58 encoding.", []))

    # Wrong encoding for version (v1 encoded as Bech32 instead of Bech32m)
    invalid.append((encode_wrong_encoding(1, v1_32, use_bech32=True),
                    "Version 1+ witness address must use Bech32m checksum", []))
    # Wrong encoding for version (v0 encoded as Bech32m instead of Bech32)
    invalid.append((encode_wrong_encoding(0, v0_20, use_bech32=False),
                    "Version 0 witness address must use Bech32 checksum", []))

    # Invalid v0 program sizes
    data_v0_16 = [0] + convertbits(list(bytes(16)), 8, 5)
    invalid.append((bech32_encode(Encoding.BECH32, HRP, data_v0_16),
                    "Invalid Bech32 v0 address program size (16 bytes), per BIP141", []))
    # Invalid generic program size (too long)
    data_v1_41 = [1] + convertbits(list(bytes(41)), 8, 5)
    invalid.append((bech32_encode(Encoding.BECH32M, HRP, data_v1_41),
                    "Invalid Bech32 address program size (41 bytes)", []))

    # Invalid Base 32 character inside data
    bad_char_addr, bad_pos = with_invalid_char(valid[2][0], index_from_sep=5)
    invalid.append((bad_char_addr, "Invalid Base 32 character", [bad_pos]))

    # Invalid witness version (>16)
    data = [17] + convertbits([0, 1], 8, 5)
    invalid.append((bech32_encode(Encoding.BECH32M, HRP, data),
                    "Invalid Bech32 address witness version", []))

    # Empty Bech32 data section
    invalid.append((empty_data_bech32(), "Empty Bech32 data section", []))

    # Mixed case (flip first data letter 'q' to 'Q')
    mixed_addr, mix_pos = with_mixed_case(valid[0][0], index_from_sep=0)
    invalid.append((mixed_addr, "Invalid character or mixed case", [mix_pos]))

    return invalid, valid


INVALID_DATA, VALID_DATA = make_test_vectors()


class ValidateAddressMainTest(BellscoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.chain = ""  # main
        self.num_nodes = 1
        self.extra_args = [["-prune=899"]] * self.num_nodes

    def check_valid(self, addr, spk):
        info = self.nodes[0].validateaddress(addr)
        assert_equal(info["isvalid"], True)
        assert_equal(info["scriptPubKey"], spk)
        assert "error" not in info
        assert "error_locations" not in info

    def check_invalid(self, addr, error_str, error_locations):
        res = self.nodes[0].validateaddress(addr)
        if res.get("isvalid") is True:
            self.log.info("Unexpectedly valid address: %s resp=%s", addr, res)
        assert_equal(res["isvalid"], False)
        assert_equal(res["error"], error_str)
        assert_equal(res["error_locations"], error_locations)

    def test_validateaddress(self):
        for (addr, error, locs) in INVALID_DATA:
            self.log.info("Checking invalid: %s", addr)
            self.check_invalid(addr, error, locs)
        for (addr, spk) in VALID_DATA:
            self.check_valid(addr, spk)

    def run_test(self):
        self.test_validateaddress()


if __name__ == "__main__":
    ValidateAddressMainTest(__file__).main()
