#!/usr/bin/env python3
# Copyright (c) 2015-2022 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test block processing."""
import copy
import time
import os
import binascii

from test_framework.blocktools import (
    create_block,
    create_coinbase,
    create_tx_with_script,
    get_legacy_sigopcount_block,
    MAX_BLOCK_SIGOPS,
)
from test_framework.messages import (
    CBlock,
    COIN,
    COutPoint,
    CTransaction,
    CTxIn,
    CTxOut,
    MAX_BLOCK_WEIGHT,
    SEQUENCE_FINAL,
    uint256_from_compact,
    uint256_from_str,
)
from test_framework.p2p import P2PDataStore
from test_framework.script import (
    CScript,
    MAX_SCRIPT_ELEMENT_SIZE,
    OP_2DUP,
    OP_CHECKMULTISIG,
    OP_CHECKMULTISIGVERIFY,
    OP_CHECKSIG,
    OP_CHECKSIGVERIFY,
    OP_ELSE,
    OP_ENDIF,
    OP_DROP,
    OP_FALSE,
    OP_IF,
    OP_INVALIDOPCODE,
    OP_RETURN,
    OP_TRUE,
    sign_input_legacy,
)
from test_framework.script_util import (
    script_to_p2sh_script,
)
from test_framework.test_framework import BellscoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_greater_than,
)
from test_framework.wallet_util import generate_keypair
from data import invalid_txs


class CBrokenBlock(CBlock):
    def initialize(self, base_block):
        self.vtx = copy.deepcopy(base_block.vtx)
        self.hashMerkleRoot = self.calc_merkle_root()

    def serialize(self, with_witness=False):
        r = b""
        r += super().serialize()
        r += (255).to_bytes(1, "little") + len(self.vtx).to_bytes(8, "little")
        for tx in self.vtx:
            if with_witness:
                r += tx.serialize_with_witness()
            else:
                r += tx.serialize_without_witness()
        return r

    def normal_serialize(self):
        return super().serialize()


DUPLICATE_COINBASE_SCRIPT_SIG = b'\x01\x78'


class FullBlockTest(BellscoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.extra_args = [[
            '-acceptnonstdtxn=1',
            '-testactivationheight=bip34@2',
            '-par=1',
        ]]

    COINBASE_MATURITY = 30

    def run_test(self):
        node = self.nodes[0]

        self.log.info("Bootstrapping P2P connection")
        self.bootstrap_p2p()

        self.block_heights = {}
        self.coinbase_key, self.coinbase_pubkey = generate_keypair()
        self.tip = None
        self.blocks = {}
        self.genesis_hash = int(node.getbestblockhash(), 16)
        self.block_heights[self.genesis_hash] = 0
        self.spendable_outputs = []
        self.tx_to_block_hash = {}

        self.log.info("Creating and sending initial blocks")
        b_dup_cb = self.next_block('dup_cb')
        b_dup_cb.vtx[0].vin[0].scriptSig = DUPLICATE_COINBASE_SCRIPT_SIG
        b_dup_cb.vtx[0].rehash()
        duplicate_tx = b_dup_cb.vtx[0]
        b_dup_cb = self.update_block('dup_cb', [])
        self.send_blocks([b_dup_cb])

        b0 = self.next_block(0)
        self.save_spendable_output()
        self.send_blocks([b0])
        self.log.info("Initial blocks sent")

        blocks = []
        for i in range(self.COINBASE_MATURITY + 30): # Generate a healthy buffer of outputs
            blocks.append(self.next_block(f"maturitybuffer.{i}"))
            self.save_spendable_output()
        self.send_blocks(blocks)

        out = [self.get_spendable_output() for _ in range(self.COINBASE_MATURITY + 31)]

        b1 = self.next_block(1, spend=out.pop(0))
        self.save_spendable_output()

        utxo_for_fork_1 = out.pop(0)
        b2 = self.next_block(2, spend=utxo_for_fork_1)
        self.save_spendable_output()
        self.send_blocks([b1, b2])

        self.log.info("Reject a block spending an immature coinbase.")
        self.move_tip(2)
        current_tip_height = self.block_heights[self.tip.sha256]
        immature_spend_tx = self.pick_immature_out(current_tip_height)
        b3 = self.next_block(3, spend=immature_spend_tx)
        self.send_blocks([b3], success=False, reject_reason='bad-txns-premature-spend-of-coinbase', reconnect=True)

        self.log.info("Reject a block spending an immature coinbase (on a forked chain)")
        self.move_tip(1)
        b4 = self.next_block(4, spend=out.pop(0))
        self.save_spendable_output()
        self.send_blocks([b4], False)
        fork_tip_height = self.block_heights[self.tip.sha256]
        immature_spend_tx_fork = self.pick_immature_out(fork_tip_height)
        b5 = self.next_block(5, spend=immature_spend_tx_fork)
        self.send_blocks([b5], success=False, reject_reason='bad-txns-premature-spend-of-coinbase', reconnect=True)

        self.move_tip(2)

        attempt_spend_tx = out.pop(0)
        for TxTemplate in invalid_txs.iter_all_templates():
            template = TxTemplate(spend_tx=attempt_spend_tx)
            if template.valid_in_block: continue
            self.log.info(f"Reject block with invalid tx: {TxTemplate.__name__}")
            blockname = f"for_invalid.{TxTemplate.__name__}"
            bad_block = self.next_block(blockname)
            bad_tx = template.get_tx()
            if TxTemplate != invalid_txs.InputMissing:
                self.sign_tx(bad_tx, attempt_spend_tx)
            bad_tx.rehash()
            bad_block = self.update_block(blockname, [bad_tx])
            self.send_blocks([bad_block], success=False, reject_reason=(template.block_reject_reason or template.reject_reason), reconnect=True, timeout=5)
            self.move_tip(2)

        self.log.info("Don't reorg to a chain of the same length")
        self.move_tip(1)
        b6 = self.next_block(6, spend=utxo_for_fork_1)
        txout_b6_reorged = b6.vtx[1]
        self.send_blocks([b6], False)

        self.log.info("Reorg to a longer chain")
        b7 = self.next_block(7, spend=out.pop(0))
        self.save_spendable_output()
        self.send_blocks([b7])
        assert_equal(node.getbestblockhash(), b7.hash)

        self.move_tip(2)
        utxo_for_reorg_back = out.pop(0)
        b8 = self.next_block(8, spend=utxo_for_reorg_back)
        self.save_spendable_output()
        self.send_blocks([b8], False)

        self.log.info("Reorg back to the original chain")
        utxo_on_b8_chain = b8.vtx[1]
        b9 = self.next_block(9, spend=out.pop(0))
        self.save_spendable_output()
        self.send_blocks([b9], True)
        assert_equal(node.getbestblockhash(), b9.hash)

        self.log.info("Reject a chain with a double spend, even if it is longer")
        self.move_tip(8)
        b10 = self.next_block(10, spend=utxo_for_reorg_back)
        self.send_blocks([b10], False)
        b11 = self.next_block(11, spend=out.pop(0))
        self.save_spendable_output()
        self.send_blocks([b11], False, reconnect=True)
        assert_equal(node.getbestblockhash(), b9.hash)

        self.log.info("Reject a block where the miner creates too much coinbase reward")
        self.move_tip(9)
        b12 = self.next_block(12, spend=out.pop(0), additional_coinbase_value=1)
        self.save_spendable_output()
        self.send_blocks([b12], success=False, reject_reason='bad-cb-amount', reconnect=True)
        assert_equal(node.getbestblockhash(), b9.hash)

        self.log.info("Reject a chain where the miner creates too much coinbase reward, even if the chain is longer")
        self.move_tip(8)
        b13 = self.next_block(13, spend=utxo_on_b8_chain)
        self.send_blocks([b13], False)
        b14 = self.next_block(14, spend=out.pop(0), additional_coinbase_value=1)
        self.send_blocks([b14], success=False, reject_reason='bad-cb-amount', reconnect=True)
        assert_equal(node.getbestblockhash(), b9.hash)

        self.log.info("Reject a chain where the miner creates too much coinbase reward, even if the chain is longer (on a forked chain)")
        self.move_tip(8)
        b15 = self.next_block(15, spend=utxo_on_b8_chain)
        self.save_spendable_output()
        utxo_from_b15 = b15.vtx[0]
        b16 = self.next_block(16, spend=out.pop(0))
        self.save_spendable_output()
        utxo_from_b16 = b16.vtx[0]
        b17 = self.next_block(17, spend=out.pop(0), additional_coinbase_value=1)
        self.save_spendable_output()
        self.send_blocks([b15, b16, b17], success=False, reject_reason='bad-cb-amount', reconnect=True)

        assert_equal(node.getbestblockhash(), b16.hash)

        self.log.info("All tests passed!")


    def pick_immature_out(self, current_tip_height):
        for out_block in reversed(self.spendable_outputs):
            out_tx = out_block.vtx[0]
            coinbase_height = self.block_heights[out_block.sha256]
            confirmations = current_tip_height - coinbase_height + 1
            if 0 < confirmations < self.COINBASE_MATURITY:
                self.log.info(f"Picking immature output from height {coinbase_height} ({confirmations} confs) relative to tip height {current_tip_height}")
                return out_tx
        raise AssertionError(f"Could not find an output with < {self.COINBASE_MATURITY} confirmations.")

    def add_transactions_to_block(self, block, tx_list):
        [tx.rehash() for tx in tx_list]
        block.vtx.extend(tx_list)

    def create_tx(self, spend_tx, n, value, output_script=None):
        if output_script is None:
            output_script = CScript([OP_TRUE, OP_DROP] * 15 + [OP_TRUE])
        return create_tx_with_script(spend_tx, n, amount=value, output_script=output_script)

    def sign_tx(self, tx, spend_tx):
        scriptPubKey = bytearray(spend_tx.vout[0].scriptPubKey)
        if (scriptPubKey[0] == OP_TRUE):
            tx.vin[0].scriptSig = CScript()
            return
        sign_input_legacy(tx, 0, spend_tx.vout[0].scriptPubKey, self.coinbase_key)

    def create_and_sign_transaction(self, spend_tx, value, output_script=None):
        if output_script is None:
            output_script = CScript([OP_TRUE])
        tx = self.create_tx(spend_tx, 0, value, output_script=output_script)
        self.sign_tx(tx, spend_tx)
        tx.rehash()
        return tx

    def next_block(self, number, spend=None, additional_coinbase_value=0, *, script=None, version=4):
        if script is None:
            script = CScript([OP_TRUE])
        if self.tip is None:
            base_block_hash = self.genesis_hash
            block_time = int(time.time()) + 1
        else:
            base_block_hash = self.tip.sha256
            block_time = self.tip.nTime + 1
        height = self.block_heights[base_block_hash] + 1
        coinbase = create_coinbase(height, self.coinbase_pubkey)
        coinbase.vout[0].nValue += additional_coinbase_value
        coinbase.rehash()
        tx_list = []
        if spend is not None:
            coinbase.vout[0].nValue += spend.vout[0].nValue - 1
            coinbase.rehash()
            tx = self.create_tx(spend, 0, 1, output_script=script)
            self.sign_tx(tx, spend)
            tx.rehash()
            tx_list.append(tx)
        
        block = create_block(base_block_hash, coinbase, block_time, version=version, txlist=tx_list)
        block.solve()
        self.tip = block
        self.block_heights[block.sha256] = height

        if not (isinstance(number, str) and "for_invalid" in number):
            if number in self.blocks:
                self.log.warning(f"Warning: Overwriting block {number}")
            self.blocks[number] = block
        else:
            self.blocks[number] = block
            
        return block

    def save_spendable_output(self):
        self.spendable_outputs.append(self.tip)
        self.tx_to_block_hash[self.tip.vtx[0].hash] = self.tip.sha256

    def get_spendable_output(self):
        return self.spendable_outputs.pop(0).vtx[0]

    def move_tip(self, number):
        self.tip = self.blocks[number]

    def update_block(self, block_number, new_transactions):
        block = self.blocks.get(block_number)
        if block is None:
            block = self.tip

        self.add_transactions_to_block(block, new_transactions)
        old_sha256 = block.sha256
        block.hashMerkleRoot = block.calc_merkle_root()
        block.solve()
        self.tip = block
        
        if block.sha256 != old_sha256 and old_sha256 in self.block_heights:
            self.block_heights[block.sha256] = self.block_heights.pop(old_sha256)
        
        if block_number in self.blocks:
             self.blocks[block_number] = block

        return block

    def bootstrap_p2p(self, timeout=10):
        self.helper_peer = self.nodes[0].add_p2p_connection(P2PDataStore())
        self.helper_peer.wait_for_getheaders(timeout=timeout)

    def reconnect_p2p(self, timeout=60):
        if self.helper_peer and self.helper_peer.is_connected:
            self.nodes[0].disconnect_p2ps()
        self.bootstrap_p2p(timeout=timeout)

    def send_blocks(self, blocks, success=True, reject_reason=None, force_send=False, reconnect=False, timeout=960):
        self.helper_peer.send_blocks_and_test(blocks, self.nodes[0], success=success, reject_reason=reject_reason, force_send=force_send, timeout=timeout, expect_disconnect=reconnect)
        if reconnect:
            self.reconnect_p2p(timeout=timeout)


if __name__ == '__main__':
    FullBlockTest(__file__).main()