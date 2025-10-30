#!/usr/bin/env python3
# Copyright (c) 2020-2022 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test coinstatsindex across nodes.

Test that the values returned by gettxoutsetinfo are consistent
between a node running the coinstatsindex and a node without
the index.
"""

from decimal import Decimal

from test_framework.blocktools import (
    COINBASE_MATURITY,
    create_block,
    create_coinbase,
)
from test_framework.messages import (
    COIN,
    CTxOut,
)
from test_framework.script import (
    CScript,
    OP_FALSE,
    OP_RETURN,
)
from test_framework.test_framework import BellscoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
)
from test_framework.wallet import (
    MiniWallet,
    getnewdestination,
)


class CoinStatsIndexTest(BellscoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2
        self.supports_cli = False
        self.extra_args = [
            [],
            ["-coinstatsindex"]
        ]

    def run_test(self):
        self.wallet = MiniWallet(self.nodes[0])
        self._test_init_index_after_reorg()
        self._test_coin_stats_index()
        self._test_use_index_option()
        self._test_reorg_index()
        self._test_index_rejects_hash_serialized()

    def get_subsidy(self, node, height):
        subsidy = node.getblockstats(height, ['subsidy'])['subsidy']
        return Decimal(str(subsidy)) / COIN

    def block_sanity_check(self, node, height, block_info):
        block_subsidy = self.get_subsidy(node, height)
        assert_equal(
            block_info['prevout_spent'] + block_subsidy,
            block_info['new_outputs_ex_coinbase'] + block_info['coinbase'] + block_info['unspendable']
        )

    def sync_index_node(self):
        self.wait_until(lambda: self.nodes[1].getindexinfo()['coinstatsindex']['synced'] is True)

    def _test_coin_stats_index(self):
        node = self.nodes[0]
        index_node = self.nodes[1]
        # Both none and muhash options allow the usage of the index
        index_hash_options = ['none', 'muhash']
        self.connect_nodes(0, 1)
        self.wait_until(lambda: len(node.getpeerinfo()) > 0)

        # Generate a normal transaction and mine it
        self.generate(self.wallet, COINBASE_MATURITY + 1)
        self_transfer_tx = self.wallet.send_self_transfer(from_node=node)
        self_transfer_output_value = self_transfer_tx['new_utxo']['value']
        self_transfer_fee = self_transfer_tx['fee']
        self_transfer_prevout = self_transfer_output_value + self_transfer_fee
        self.generate(node, 1)

        self.log.info("Test that gettxoutsetinfo() output is consistent with or without coinstatsindex option")
        res0 = node.gettxoutsetinfo('none')
        self.base_height = res0['height']
        self.height_after_opreturn = self.base_height + 6
        self.height_custom_cb = self.base_height + 7
        self.height_wait = self.base_height + 8
        self.height_hash_serialized = self.base_height + 9
        self.height_reorg_target = self.base_height + 10
        self.reconsider_block_height = max(self.base_height - 3, 0)

        # The fields 'disk_size' and 'transactions' do not exist on the index
        del res0['disk_size'], res0['transactions']

        for hash_option in index_hash_options:
            res1 = index_node.gettxoutsetinfo(hash_option)
            # The fields 'block_info' and 'total_unspendable_amount' only exist on the index
            del res1['block_info'], res1['total_unspendable_amount']
            res1.pop('muhash', None)

            # Everything left should be the same
            assert_equal(res1, res0)

        self.log.info("Test that gettxoutsetinfo() can get fetch data on specific heights with index")

        # Generate a new tip
        self.generate(node, 5)

        for hash_option in index_hash_options:
            # Fetch old stats by height
            res2 = index_node.gettxoutsetinfo(hash_option, self.base_height)
            del res2['block_info'], res2['total_unspendable_amount']
            res2.pop('muhash', None)
            assert_equal(res0, res2)

            # Fetch old stats by hash
            res3 = index_node.gettxoutsetinfo(hash_option, res0['bestblock'])
            del res3['block_info'], res3['total_unspendable_amount']
            res3.pop('muhash', None)
            assert_equal(res0, res3)

            # It does not work without coinstatsindex
            assert_raises_rpc_error(-8, "Querying specific block heights requires coinstatsindex", node.gettxoutsetinfo, hash_option, self.base_height)

        self.log.info("Test gettxoutsetinfo() with index and verbose flag")

        genesis_subsidy = self.get_subsidy(index_node, 0)
        block_subsidy = self.get_subsidy(index_node, self.base_height)

        for hash_option in index_hash_options:
            # Genesis block is unspendable
            res4 = index_node.gettxoutsetinfo(hash_option, 0)
            assert_equal(res4['total_unspendable_amount'], genesis_subsidy)
            assert_equal(res4['block_info'], {
                'unspendable': genesis_subsidy,
                'prevout_spent': 0,
                'new_outputs_ex_coinbase': 0,
                'coinbase': 0,
                'unspendables': {
                    'genesis_block': genesis_subsidy,
                    'bip30': 0,
                    'scripts': 0,
                    'unclaimed_rewards': 0
                }
            })
            self.block_sanity_check(index_node, 0, res4['block_info'])

            # Test an older block height that included a normal tx
            res5 = index_node.gettxoutsetinfo(hash_option, self.base_height)
            assert_equal(res5['total_unspendable_amount'], genesis_subsidy)
            assert_equal(res5['block_info'], {
                'unspendable': 0,
                'prevout_spent': self_transfer_prevout,
                'new_outputs_ex_coinbase': self_transfer_output_value,
                'coinbase': block_subsidy + self_transfer_fee,
                'unspendables': {
                    'genesis_block': 0,
                    'bip30': 0,
                    'scripts': 0,
                    'unclaimed_rewards': 0,
                }
            })
            self.block_sanity_check(index_node, self.base_height, res5['block_info'])

        quantize_unit = Decimal('0.00000001')
        send_ratio = Decimal('0.42')
        burn_ratio = Decimal('0.0002')
        send_to_fee = (Decimal(1000) / COIN).quantize(quantize_unit)
        send_amount = (block_subsidy * send_ratio).quantize(quantize_unit)
        burn_fee = (block_subsidy * burn_ratio).quantize(quantize_unit)
        if burn_fee == 0:
            burn_fee = quantize_unit
        burn_fee = min(burn_fee, send_amount / 2)
        op_return_amount = (send_amount - burn_fee).quantize(quantize_unit)
        send_amount_sat = int((send_amount * COIN).to_integral_exact())
        op_return_sat = int((op_return_amount * COIN).to_integral_exact())
        tx2_value_str = format(op_return_amount, 'f')
        prevout_spent_after_opreturn = (block_subsidy + send_amount).quantize(quantize_unit)
        new_outputs_ex_coinbase_after_opreturn = (block_subsidy - send_to_fee).quantize(quantize_unit)
        coinbase_after_opreturn = (block_subsidy + send_to_fee + burn_fee).quantize(quantize_unit)
        total_unspendable_after_opreturn = (genesis_subsidy + op_return_amount).quantize(quantize_unit)

        # Generate and send a normal tx with two outputs
        tx1 = self.wallet.send_to(
            from_node=node,
            scriptPubKey=self.wallet.get_scriptPubKey(),
            amount=send_amount_sat,
        )

        # Find the right position of the 21 BTC output
        tx1_out_21 = self.wallet.get_utxo(txid=tx1["txid"], vout=tx1["sent_vout"])

        # Generate and send another tx with an OP_RETURN output (which is unspendable)
        tx2 = self.wallet.create_self_transfer(utxo_to_spend=tx1_out_21)['tx']
        tx2.vout = [CTxOut(op_return_sat, CScript([OP_RETURN] + [OP_FALSE] * 30))]
        tx2.rehash()
        tx2_hex = tx2.serialize().hex()
        self.nodes[0].sendrawtransaction(tx2_hex, 0, tx2_value_str)

        # Include both txs in a block
        self.generate(self.nodes[0], 1)

        coinbase_primary = (block_subsidy * Decimal('0.7')).quantize(quantize_unit)
        coinbase_secondary = (block_subsidy * Decimal('0.1')).quantize(quantize_unit)
        coinbase_claimed = (coinbase_primary + coinbase_secondary).quantize(quantize_unit)
        unclaimed_reward = (block_subsidy - coinbase_claimed).quantize(quantize_unit)
        coinbase_primary_sat = int((coinbase_primary * COIN).to_integral_exact())
        coinbase_secondary_sat = int((coinbase_secondary * COIN).to_integral_exact())
        total_unspendable_after_custom_coinbase = (total_unspendable_after_opreturn + unclaimed_reward).quantize(quantize_unit)
        for hash_option in index_hash_options:
            # Check all amounts were registered correctly
            res6 = index_node.gettxoutsetinfo(hash_option, self.height_after_opreturn)
            assert_equal(res6['total_unspendable_amount'], total_unspendable_after_opreturn)
            assert_equal(res6['block_info'], {
                'unspendable': op_return_amount,
                'prevout_spent': prevout_spent_after_opreturn,
                'new_outputs_ex_coinbase': new_outputs_ex_coinbase_after_opreturn,
                'coinbase': coinbase_after_opreturn,
                'unspendables': {
                    'genesis_block': 0,
                    'bip30': 0,
                    'scripts': op_return_amount,
                    'unclaimed_rewards': 0,
                }
            })
            self.block_sanity_check(index_node, self.height_after_opreturn, res6['block_info'])

        # Create a coinbase that does not claim full subsidy and also
        # has two outputs
        cb = create_coinbase(self.height_custom_cb)
        cb.vout[0].nValue = coinbase_primary_sat
        cb.vout.append(CTxOut(coinbase_secondary_sat, CScript([OP_FALSE])))
        cb.rehash()

        # Generate a block that includes previous coinbase
        tip = self.nodes[0].getbestblockhash()
        block_time = self.nodes[0].getblock(tip)['time'] + 1
        block = create_block(int(tip, 16), cb, block_time, use_auxpow=True)
        self.nodes[0].submitblock(block.serialize().hex())
        self.sync_all()

        for hash_option in index_hash_options:
            res7 = index_node.gettxoutsetinfo(hash_option, self.height_custom_cb)
            assert_equal(res7['total_unspendable_amount'], total_unspendable_after_custom_coinbase)
            assert_equal(res7['block_info'], {
                'unspendable': unclaimed_reward,
                'prevout_spent': 0,
                'new_outputs_ex_coinbase': 0,
                'coinbase': coinbase_claimed,
                'unspendables': {
                    'genesis_block': 0,
                    'bip30': 0,
                    'scripts': 0,
                    'unclaimed_rewards': unclaimed_reward
                }
            })
            self.block_sanity_check(index_node, self.height_custom_cb, res7['block_info'])

        self.log.info("Test that the index is robust across restarts")

        res8 = index_node.gettxoutsetinfo('muhash')
        self.restart_node(1, extra_args=self.extra_args[1])
        self.connect_nodes(0, 1)
        self.wait_until(lambda: len(node.getpeerinfo()) > 0)
        res9 = index_node.gettxoutsetinfo('muhash')
        assert_equal(res8, res9)

        self.generate(node, 1)
        res10 = index_node.gettxoutsetinfo('muhash')
        assert res8['txouts'] < res10['txouts']

        self.log.info("Test that the index works with -reindex")

        self.restart_node(1, extra_args=["-coinstatsindex", "-reindex"])
        self.connect_nodes(0, 1)
        self.wait_until(lambda: len(node.getpeerinfo()) > 0)
        self.sync_all()
        self.sync_index_node()
        res11 = index_node.gettxoutsetinfo('muhash')
        assert_equal(res11, res10)

        self.log.info("Test that the index works with -reindex-chainstate")

        self.restart_node(1, extra_args=["-coinstatsindex", "-reindex-chainstate"])
        self.connect_nodes(0, 1)
        self.wait_until(lambda: len(node.getpeerinfo()) > 0)
        self.sync_all()
        self.sync_index_node()
        res12 = index_node.gettxoutsetinfo('muhash')
        assert_equal(res12, res10)

        self.log.info("Test obtaining info for a non-existent block hash")
        assert_raises_rpc_error(-5, "Block not found", index_node.gettxoutsetinfo, hash_type="none", hash_or_height="ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff", use_index=True)

    def _test_use_index_option(self):
        self.log.info("Test use_index option for nodes running the index")

        self.connect_nodes(0, 1)
        self.nodes[0].waitforblockheight(self.height_wait)
        res = self.nodes[0].gettxoutsetinfo('muhash')
        option_res = self.nodes[1].gettxoutsetinfo(hash_type='muhash', hash_or_height=None, use_index=False)
        del res['disk_size'], option_res['disk_size']
        assert_equal(res, option_res)

    def _test_reorg_index(self):
        self.log.info("Test that index can handle reorgs")

        # Generate two block, let the index catch up, then invalidate the blocks
        index_node = self.nodes[1]
        reorg_blocks = self.generatetoaddress(index_node, 2, getnewdestination()[2])
        reorg_block = reorg_blocks[1]
        self.sync_index_node()
        res_invalid = index_node.gettxoutsetinfo('muhash')
        index_node.invalidateblock(reorg_blocks[0])
        assert_equal(index_node.gettxoutsetinfo('muhash')['height'], self.height_wait)

        # Add two new blocks
        block = self.generate(index_node, 2, sync_fun=self.no_op)[1]
        res = index_node.gettxoutsetinfo(hash_type='muhash', hash_or_height=None, use_index=False)

        # Test that the result of the reorged block is not returned for its old block height
        res2 = index_node.gettxoutsetinfo(hash_type='muhash', hash_or_height=self.height_reorg_target)
        assert_equal(res["bestblock"], block)
        assert_equal(res["muhash"], res2["muhash"])
        assert res["muhash"] != res_invalid["muhash"]

        # Test that requesting reorged out block by hash is still returning correct results
        res_invalid2 = index_node.gettxoutsetinfo(hash_type='muhash', hash_or_height=reorg_block)
        assert_equal(res_invalid2["muhash"], res_invalid["muhash"])
        assert res["muhash"] != res_invalid2["muhash"]

        # Add another block, so we don't depend on reconsiderblock remembering which
        # blocks were touched by invalidateblock
        self.generate(index_node, 1)

        # Ensure that removing and re-adding blocks yields consistent results
        block = index_node.getblockhash(self.reconsider_block_height)
        index_node.invalidateblock(block)
        index_node.reconsiderblock(block)
        res3 = index_node.gettxoutsetinfo(hash_type='muhash', hash_or_height=self.height_reorg_target)
        assert_equal(res2, res3)

    def _test_index_rejects_hash_serialized(self):
        self.log.info("Test that the rpc raises if the legacy hash is passed with the index")

        msg = "hash_serialized_3 hash type cannot be queried for a specific block"
        assert_raises_rpc_error(-8, msg, self.nodes[1].gettxoutsetinfo, hash_type='hash_serialized_3', hash_or_height=self.height_hash_serialized)

        for use_index in {True, False, None}:
            assert_raises_rpc_error(-8, msg, self.nodes[1].gettxoutsetinfo, hash_type='hash_serialized_3', hash_or_height=self.height_hash_serialized, use_index=use_index)

    def _test_init_index_after_reorg(self):
        self.log.info("Test a reorg while the index is deactivated")
        index_node = self.nodes[1]
        block = self.nodes[0].getbestblockhash()
        self.generate(index_node, 2, sync_fun=self.no_op)
        self.sync_index_node()

        # Restart without index
        self.restart_node(1, extra_args=["-reindex"])
        self.connect_nodes(0, 1)
        index_node.invalidateblock(block)
        self.generatetoaddress(index_node, 5, getnewdestination()[2])
        res = index_node.gettxoutsetinfo(hash_type='muhash', hash_or_height=None, use_index=False)

        # Restart with index that still has its best block on the old chain
        self.restart_node(1, extra_args=self.extra_args[1] + ["-reindex"])
        self.sync_index_node()
        res1 = index_node.gettxoutsetinfo(hash_type='muhash', hash_or_height=None, use_index=True)
        res2 = index_node.gettxoutsetinfo(hash_type='muhash', hash_or_height=None, use_index=False)
        assert_equal(res1["muhash"], res2["muhash"])



if __name__ == '__main__':
    CoinStatsIndexTest(__file__).main()
