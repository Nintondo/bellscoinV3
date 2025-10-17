#!/usr/bin/env python3
# Copyright (c) 2020-2022 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test datacarrier functionality"""
from test_framework.messages import (
    CTxOut,
    MAX_OP_RETURN_RELAY,
    WITNESS_SCALE_FACTOR,
    ser_compact_size,
)
from test_framework.script import (
    CScript,
    OP_RETURN,
)
from test_framework.test_framework import BellscoinTestFramework
from test_framework.test_node import TestNode
from test_framework.util import assert_raises_rpc_error
from test_framework.wallet import MiniWallet

from random import randbytes


class DataCarrierTest(BellscoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 4
        self.extra_args = [
            [],
            ["-datacarrier=0"],
            ["-datacarrier=1", f"-datacarriersize={MAX_OP_RETURN_RELAY // 2}"],
            ["-datacarrier=1", f"-datacarriersize=2"],
        ]

    def test_null_data_transaction(self, node: TestNode, data, success: bool, expected_error: str = "datacarrier") -> None:
        tx = self.wallet.create_self_transfer(fee_rate=0)["tx"]
        data = [] if data is None else [data]
        tx.vout.append(CTxOut(nValue=0, scriptPubKey=CScript([OP_RETURN] + data)))
        tx.vout[0].nValue -= tx.get_vsize()  # simply pay 1sat/vbyte fee

        tx_hex = tx.serialize().hex()

        if success:
            self.wallet.sendrawtransaction(from_node=node, tx_hex=tx_hex)
            assert tx.rehash() in node.getrawmempool(True), f'{tx_hex} not in mempool'
        else:
            assert_raises_rpc_error(-26, expected_error, self.wallet.sendrawtransaction, from_node=node, tx_hex=tx_hex)

    def run_test(self):
        self.wallet = MiniWallet(self.nodes[0])

        template_utxo = self.wallet.get_utxo(mark_as_spent=False)
        template_tx = self.wallet.create_self_transfer(utxo_to_spend=template_utxo, fee_rate=0)["tx"]
        base_vsize = template_tx.get_vsize()

        def script_size(payload_length: int) -> int:
            return len(CScript([OP_RETURN, b"\x00" * payload_length]))

        def tx_vsize(payload_length: int) -> int:
            script_len = script_size(payload_length)
            return base_vsize + 8 + len(ser_compact_size(script_len)) + script_len

        max_tx_weight = MAX_OP_RETURN_RELAY * WITNESS_SCALE_FACTOR
        max_tx_vbytes = max_tx_weight // WITNESS_SCALE_FACTOR

        def max_payload_length_for(script_limit: int) -> int:
            payload_length = script_limit
            while payload_length >= 0:
                script_len = script_size(payload_length)
                if script_len <= script_limit and tx_vsize(payload_length) <= max_tx_vbytes:
                    return payload_length
                payload_length -= 1
            raise RuntimeError("No payload length satisfies the constraints")

        def rejection_reason(payload_length: int, script_limit: int) -> str:
            if tx_vsize(payload_length) > max_tx_vbytes:
                return "tx-size"
            if script_size(payload_length) > script_limit:
                return "datacarrier"
            return "scriptpubkey"

        default_limit = MAX_OP_RETURN_RELAY
        reduced_limit = MAX_OP_RETURN_RELAY // 2

        default_payload_length = max_payload_length_for(default_limit)
        node2_payload_length = max_payload_length_for(reduced_limit)

        default_size_data = randbytes(default_payload_length)
        too_long_data = randbytes(default_payload_length + 1)
        small_data = randbytes(node2_payload_length)
        node2_too_long_data = randbytes(node2_payload_length + 1)
        one_byte = randbytes(1)
        zero_bytes = randbytes(0)

        self.log.info("Testing null data transaction with default -datacarrier and -datacarriersize values.")
        self.test_null_data_transaction(node=self.nodes[0], data=default_size_data, success=True)

        self.log.info("Testing a null data transaction larger than allowed by the default -datacarriersize value.")
        self.test_null_data_transaction(
            node=self.nodes[0],
            data=too_long_data,
            success=False,
            expected_error=rejection_reason(len(too_long_data), default_limit),
        )

        self.log.info("Testing a null data transaction with -datacarrier=false.")
        self.test_null_data_transaction(
            node=self.nodes[1],
            data=default_size_data,
            success=False,
            expected_error="datacarrier",
        )

        self.log.info("Testing a null data transaction with a size larger than accepted by -datacarriersize.")
        self.test_null_data_transaction(
            node=self.nodes[2],
            data=node2_too_long_data,
            success=False,
            expected_error=rejection_reason(len(node2_too_long_data), reduced_limit),
        )

        self.log.info("Testing a null data transaction with a size smaller than accepted by -datacarriersize.")
        self.test_null_data_transaction(node=self.nodes[2], data=small_data, success=True)

        self.log.info("Testing a null data transaction with no data.")
        self.test_null_data_transaction(node=self.nodes[0], data=None, success=True)
        self.test_null_data_transaction(node=self.nodes[1], data=None, success=False)
        self.test_null_data_transaction(node=self.nodes[2], data=None, success=True)
        self.test_null_data_transaction(node=self.nodes[3], data=None, success=True)

        self.log.info("Testing a null data transaction with zero bytes of data.")
        self.test_null_data_transaction(node=self.nodes[0], data=zero_bytes, success=True)
        self.test_null_data_transaction(node=self.nodes[1], data=zero_bytes, success=False)
        self.test_null_data_transaction(node=self.nodes[2], data=zero_bytes, success=True)
        self.test_null_data_transaction(node=self.nodes[3], data=zero_bytes, success=True)

        self.log.info("Testing a null data transaction with one byte of data.")
        self.test_null_data_transaction(node=self.nodes[0], data=one_byte, success=True)
        self.test_null_data_transaction(node=self.nodes[1], data=one_byte, success=False)
        self.test_null_data_transaction(node=self.nodes[2], data=one_byte, success=True)
        self.test_null_data_transaction(node=self.nodes[3], data=one_byte, success=False, expected_error="datacarrier")


if __name__ == '__main__':
    DataCarrierTest(__file__).main()
