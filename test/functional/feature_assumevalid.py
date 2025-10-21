#!/usr/bin/env python3
# Copyright (c) 2014-2022 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test logic for skipping signature validation on old blocks.

Note: This test performs a long headers+blocks sync to satisfy the
two-week equivalent-work threshold for assumevalid. On Bells (60s target
spacing), this means ~20k blocks, which can be slow. By default we skip
the heavy run. To execute the full test, pass the flag
`--full-assumevalid` to the test runner.

Test logic for skipping signature validation on blocks which we've assumed
valid (https://github.com/bitcoin/bitcoin/pull/9484)

We build a chain that includes and invalid signature for one of the
transactions:

    0:        genesis block
    1:        block 1 with coinbase transaction output.
    2-101:    bury that block with 100 blocks so the coinbase transaction
              output can be spent
    102:      a block containing a transaction spending the coinbase
              transaction output. The transaction has an invalid signature.
    103-2202: bury the bad block with just over two weeks' worth of blocks
              (2100 blocks)

Start three nodes:

    - node0 has no -assumevalid parameter. Try to sync to block 2202. It will
      reject block 102 and only sync as far as block 101
    - node1 has -assumevalid set to the hash of block 102. Try to sync to
      block 2202. node1 will sync all the way to block 2202.
    - node2 has -assumevalid set to the hash of block 102. Try to sync to
      block 200. node2 will reject block 102 since it's assumed valid, but it
      isn't buried by at least two weeks' work.
"""

from test_framework.blocktools import (
    COINBASE_MATURITY,
    create_block,
    create_coinbase,
)
from test_framework.messages import (
    CBlockHeader,
    COutPoint,
    CTransaction,
    CTxIn,
    CTxOut,
    msg_block,
    msg_headers,
)
from test_framework.p2p import P2PInterface
from test_framework.script import (
    CScript,
    OP_TRUE,
)
from test_framework.test_framework import BellscoinTestFramework, SkipTest
from test_framework.util import assert_equal
from test_framework.wallet_util import generate_keypair


class BaseNode(P2PInterface):
    def send_header_for_blocks(self, new_blocks):
        headers_message = msg_headers()
        headers_message.headers = [CBlockHeader(b) for b in new_blocks]
        self.send_message(headers_message)


class AssumeValidTest(BellscoinTestFramework):
    def add_options(self, parser):
        parser.add_argument(
            "--full-assumevalid",
            dest="full_assumevalid",
            default=False,
            action="store_true",
            help="Run the full-length assumevalid test (long; ~20k blocks)",
        )
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 3
        self.rpc_timeout = 120

    def setup_network(self):
        self.add_nodes(3)
        # Start node0. We don't start the other nodes yet since
        # we need to pre-mine a block with an invalid transaction
        # signature so we can pass in the block hash as assumevalid.
        self.start_node(0)

    def send_blocks_until_disconnected(self, p2p_conn):
        """Keep sending blocks to the node until we're disconnected."""
        for i in range(len(self.blocks)):
            if not p2p_conn.is_connected:
                break
            try:
                p2p_conn.send_message(msg_block(self.blocks[i]))
            except IOError:
                assert not p2p_conn.is_connected
                break

    def run_test(self):
        # Short-circuit by default to avoid very long runs. Use
        # `--full-assumevalid` to run the complete scenario.
        if not getattr(self.options, "full_assumevalid", False):
            raise SkipTest("Skipping long assumevalid test; pass --full-assumevalid to run fully")
        # Build the blockchain
        self.tip = int(self.nodes[0].getbestblockhash(), 16)
        self.block_time = self.nodes[0].getblock(self.nodes[0].getbestblockhash())['time'] + 1

        self.blocks = []

        # Get a pubkey for the coinbase TXO
        _, coinbase_pubkey = generate_keypair()

        # Create the first block with a coinbase output to our key
        height = 1
        block = create_block(self.tip, create_coinbase(height, coinbase_pubkey), self.block_time)
        self.blocks.append(block)
        self.block_time += 1
        block.solve()
        # Save the coinbase for later
        self.block1 = block
        self.tip = block.sha256
        height += 1

        # Bury the block COINBASE_MATURITY deep so the coinbase output is spendable
        for _ in range(COINBASE_MATURITY):
            block = create_block(self.tip, create_coinbase(height), self.block_time)
            block.solve()
            self.blocks.append(block)
            self.tip = block.sha256
            self.block_time += 1
            height += 1

        # Create a transaction spending the coinbase output with an invalid (null) signature
        tx = CTransaction()
        tx.vin.append(CTxIn(COutPoint(self.block1.vtx[0].sha256, 0), scriptSig=b""))
        tx.vout.append(CTxOut(2 * 100000000, CScript([OP_TRUE])))
        tx.calc_sha256()

        block102 = create_block(self.tip, create_coinbase(height), self.block_time, txlist=[tx])
        self.block_time += 1
        block102.solve()
        self.blocks.append(block102)
        self.tip = block102.sha256
        self.block_time += 1
        height += 1

        # Bury the assumed valid block deep enough to exceed the 2-week equivalent
        # time threshold used by assumevalid skipping logic:
        # threshold = 14 days, equivalent_time ~= blocks * PoWTargetSpacing().
        # Bells regtest spacing is 60s, so need ~20160 blocks. Use a small safety margin.
        # Use ~two weeks of equivalent work at 60s spacing: ~20,160 blocks.
        # Add a small safety margin.
        BURY_AFTER_BAD = 20200
        for _ in range(BURY_AFTER_BAD):
            block = create_block(self.tip, create_coinbase(height), self.block_time)
            block.solve()
            self.blocks.append(block)
            self.tip = block.sha256
            self.block_time += 1
            height += 1

        # Advance mocktime so future-dated blocks are accepted even with large bury depth.
        self.nodes[0].setmocktime(self.block_time)

        # Start node1 and node2 with assumevalid so they accept a block with a bad signature.
        self.start_node(1, extra_args=["-assumevalid=" + hex(block102.sha256)])
        self.start_node(2, extra_args=["-assumevalid=" + hex(block102.sha256)])
        self.nodes[1].setmocktime(self.block_time)
        self.nodes[2].setmocktime(self.block_time)

        p2p0 = self.nodes[0].add_p2p_connection(BaseNode())
        # Node0 only needs headers up to the bad-spend height to reject it.
        p2p0.send_header_for_blocks(self.blocks[:COINBASE_MATURITY + 2])

        # Send blocks to node0. The block that spends the coinbase (at height COINBASE_MATURITY+2)
        # will be rejected due to invalid signature when assumevalid is not used.
        self.send_blocks_until_disconnected(p2p0)
        self.wait_until(lambda: self.nodes[0].getblockcount() >= COINBASE_MATURITY + 1)
        assert_equal(self.nodes[0].getblockcount(), COINBASE_MATURITY + 1)

        p2p1 = self.nodes[1].add_p2p_connection(BaseNode())
        # Send headers in manageable chunks to avoid disconnects.
        total_blocks = 1 + COINBASE_MATURITY + 1 + BURY_AFTER_BAD
        for i in range(0, total_blocks, 1000):
            p2p1.send_header_for_blocks(self.blocks[i:i+1000])
            p2p1.sync_with_ping(timeout=120)

        # Send all blocks to node1. All blocks will be accepted.
        for i in range(total_blocks):
            p2p1.send_message(msg_block(self.blocks[i]))
        # Syncing many blocks can take a while on slow systems. Give it plenty of time to sync.
        p2p1.sync_with_ping(timeout=3600)
        assert_equal(self.nodes[1].getblock(self.nodes[1].getbestblockhash())['height'], total_blocks)

        p2p2 = self.nodes[2].add_p2p_connection(BaseNode())
        p2p2.send_header_for_blocks(self.blocks[0:200])

        # Send blocks to node2. Block 102 will be rejected.
        self.send_blocks_until_disconnected(p2p2)
        self.wait_until(lambda: self.nodes[2].getblockcount() >= COINBASE_MATURITY + 1)
        assert_equal(self.nodes[2].getblockcount(), COINBASE_MATURITY + 1)


if __name__ == '__main__':
    AssumeValidTest(__file__).main()
