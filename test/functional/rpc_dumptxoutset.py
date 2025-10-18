#!/usr/bin/env python3
# Copyright (c) 2019-2022 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the generation of UTXO snapshots using `dumptxoutset`.
"""

from test_framework.blocktools import COINBASE_MATURITY
from test_framework.test_framework import BellscoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
    sha256sum_file,
)


class DumptxoutsetTest(BellscoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1

    def run_test(self):
        """Test a trivial usage of the dumptxoutset RPC command."""
        node = self.nodes[0]
        mocktime = node.getblockheader(node.getblockhash(0))['time'] + 1
        node.setmocktime(mocktime)
        self.generate(node, COINBASE_MATURITY)

        FILENAME = 'txoutset.dat'
        out = node.dumptxoutset(FILENAME)
        expected_path = node.datadir_path / self.chain / FILENAME

        assert expected_path.is_file()

        # Bells regtest maturity differs from Bitcoin's. Expect snapshot at
        # the current maturity height.
        assert_equal(out['coins_written'], COINBASE_MATURITY)
        assert_equal(out['base_height'], COINBASE_MATURITY)
        assert_equal(out['path'], str(expected_path))
        
        # Blockhash should be deterministic based on mocked time.
        assert_equal(
            out['base_hash'],
            'ac62b802a9ce15378f0cf871efc7dc5d533c5ff24fc08a79b4676bde3f84949b')

        # UTXO snapshot hash should be deterministic based on mocked time.
        assert_equal(
            sha256sum_file(str(expected_path)).hex(),
            'eeeddd7d33e7bc76a9187213a425b4830d6b5ac8921fda4eae4c3e7ae72a55d6')

        assert_equal(out['txoutset_hash'], '383e274394dc15c4ff81b7a653ac845e4e59c8dd7570d4d542886a56c1bd5c57')

        # Number of chain transactions includes genesis + generated blocks
        assert_equal(out['nchaintx'], COINBASE_MATURITY + 1)

        # Specifying a path to an existing or invalid file will fail.
        assert_raises_rpc_error(
            -8, '{} already exists'.format(FILENAME),  node.dumptxoutset, FILENAME)
        invalid_path = node.datadir_path / "invalid" / "path"
        assert_raises_rpc_error(
            -8, "Couldn't open file {}.incomplete for writing".format(invalid_path), node.dumptxoutset, invalid_path)


if __name__ == '__main__':
    DumptxoutsetTest(__file__).main()
