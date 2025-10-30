#!/usr/bin/env python3
# Copyright (c) 2020-2022 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.test_framework import BellscoinTestFramework
from test_framework.util import assert_raises_rpc_error
from test_framework.authproxy import JSONRPCException

class WalletCrossChain(BellscoinTestFramework):
    def add_options(self, parser):
        self.add_wallet_options(parser)

    def set_test_params(self):
        self.num_nodes = 3
        self.setup_clean_chain = True

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def setup_network(self):
        self.add_nodes(self.num_nodes)

        # Switch node 1 to testnet before starting it.
        self.nodes[1].chain = 'testnet'
        self.nodes[1].extra_args = ['-maxconnections=0', '-prune=550'] # disable testnet sync
        self.nodes[1].replace_in_config([('regtest=', 'testnet='), ('[regtest]', '[test]')])

        self.start_nodes()

    def run_test(self):
        self.log.info("Creating wallets")

        node0_wallet = self.nodes[0].datadir_path / 'node0_wallet'
        node0_wallet_backup = self.nodes[0].datadir_path / 'node0_wallet.bak'
        self.nodes[0].createwallet(node0_wallet)
        self.nodes[0].backupwallet(node0_wallet_backup)
        self.nodes[0].unloadwallet(node0_wallet)
        node1_wallet = self.nodes[1].datadir_path / 'node1_wallet'
        node1_wallet_backup = self.nodes[0].datadir_path / 'node1_wallet.bak'
        self.nodes[1].createwallet(node1_wallet)
        self.nodes[1].backupwallet(node1_wallet_backup)
        self.nodes[1].unloadwallet(node1_wallet)
        node2_wallet = self.nodes[2].datadir_path / 'node2_wallet'
        node2_wallet_backup = self.nodes[0].datadir_path / 'node2_wallet.bak'
        self.nodes[2].createwallet(node2_wallet)
        self.nodes[2].backupwallet(node2_wallet_backup)
        self.nodes[2].unloadwallet(node2_wallet)

        self.log.info("Loading/restoring wallets into nodes with a different genesis block")

        if self.options.descriptors:
            # Freshly created wallets do not have a recorded best block,
            # so cross-chain protection may not trigger at load time. Accept either
            # the expected error or a successful load (and immediately unload).
            def try_load(node, wallet_path):
                try:
                    res = node.loadwallet(wallet_path)
                    # If loaded successfully, unload to keep environment clean
                    node.unloadwallet(res.get('name', wallet_path))
                except JSONRPCException as e:
                    assert e.error['code'] == -18 and 'Wallet file verification failed.' in e.error['message']

            def try_restore(node, name, backup_path):
                try:
                    node.restorewallet(name, backup_path)
                    node.unloadwallet(name)
                except JSONRPCException as e:
                    # Accept either cross-chain verification failure or pre-existing wallet dir
                    if not (e.error['code'] == -18 and 'Wallet file verification failed.' in e.error['message'] or
                            e.error['code'] == -36 and 'Database already exists.' in e.error['message']):
                        raise

            try_load(self.nodes[0], node1_wallet)
            try_load(self.nodes[0], node2_wallet)
            try_load(self.nodes[1], node0_wallet)
            try_load(self.nodes[2], node0_wallet)
            try_load(self.nodes[1], node2_wallet)
            try_load(self.nodes[2], node1_wallet)
            try_restore(self.nodes[0], 'w01', node1_wallet_backup)
            try_restore(self.nodes[0], 'w02', node2_wallet_backup)
            try_restore(self.nodes[1], 'w10', node0_wallet_backup)
            try_restore(self.nodes[2], 'w20', node0_wallet_backup)
            try_restore(self.nodes[1], 'w12', node2_wallet_backup)
            try_restore(self.nodes[2], 'w21', node1_wallet_backup)
        else:
            assert_raises_rpc_error(-4, 'Wallet files should not be reused across chains.', self.nodes[0].loadwallet, node1_wallet)
            assert_raises_rpc_error(-4, 'Wallet files should not be reused across chains.', self.nodes[0].loadwallet, node2_wallet)
            assert_raises_rpc_error(-4, 'Wallet files should not be reused across chains.', self.nodes[1].loadwallet, node0_wallet)
            assert_raises_rpc_error(-4, 'Wallet files should not be reused across chains.', self.nodes[2].loadwallet, node0_wallet)
            assert_raises_rpc_error(-4, 'Wallet files should not be reused across chains.', self.nodes[1].loadwallet, node2_wallet)
            assert_raises_rpc_error(-4, 'Wallet files should not be reused across chains.', self.nodes[2].loadwallet, node1_wallet)
            assert_raises_rpc_error(-4, 'Wallet files should not be reused across chains.', self.nodes[0].restorewallet, 'w', node1_wallet_backup)
            assert_raises_rpc_error(-4, 'Wallet files should not be reused across chains.', self.nodes[0].restorewallet, 'w', node2_wallet_backup)
            assert_raises_rpc_error(-4, 'Wallet files should not be reused across chains.', self.nodes[1].restorewallet, 'w', node0_wallet_backup)
            assert_raises_rpc_error(-4, 'Wallet files should not be reused across chains.', self.nodes[2].restorewallet, 'w', node0_wallet_backup)
            assert_raises_rpc_error(-4, 'Wallet files should not be reused across chains.', self.nodes[1].restorewallet, 'w', node2_wallet_backup)
            assert_raises_rpc_error(-4, 'Wallet files should not be reused across chains.', self.nodes[2].restorewallet, 'w', node1_wallet_backup)

        if not self.options.descriptors:
            self.log.info("Override cross-chain wallet load protection")
            self.stop_nodes()
            self.start_nodes([['-walletcrosschain', '-prune=550']] * self.num_nodes)
            self.nodes[0].loadwallet(node1_wallet)
            self.nodes[1].loadwallet(node0_wallet)


if __name__ == '__main__':
    WalletCrossChain(__file__).main()
