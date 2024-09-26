#!/usr/bin/env python3
# Copyright (c) 2018-2022 The Bellscoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test groth16 arguments.
"""

from test_framework.test_framework import BellscoinTestFramework
from test_framework.util import assert_equal
from decimal import Decimal
import json
from test_framework.script import (
    CScript,
    OP_CHECKGROTH16VERIFY
)

from test_framework.messages import (
    COutPoint,
    CTransaction,
    CTxIn,
    CTxOut,
)

class tx_data:
    def __init__(self, value: str, vout: str, txid: str):
        self.value = value
        self.vout = vout
        self.txid = txid

    def __repr__(self):
        return f"TxData(value={self.value}\n vout={self.vout}\n txid={self.txid})\n"

# Для правильного преобразования Decimal в строку
class DecimalEncoder(json.JSONEncoder):
    def default(self, o):
        if isinstance(o, Decimal):
            return str(o)
        return super(DecimalEncoder, self).default(o)

def print_pairs(pairs):
    for key, value in pairs:
        print(f"First: {key}, Second: {json.dumps(value, cls=DecimalEncoder, indent=4)}")

def btfl_json(json_data):
    return json.dumps(json_data, cls=DecimalEncoder, indent=4)

class Groth16VerifyTest(BellscoinTestFramework):
    def add_options(self, parser):
        self.add_wallet_options(parser)

    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1
        self.extra_args = [[
            "-walletrejectlongchains=0", "-whitelist=noban@127.0.0.1", "-maxtxfee=0.1"
        ]]
        self.supports_cli = False

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def setup_network(self, split=False):
        self.setup_nodes()

    # Return [{"value", "txid"}, ...]
    def get_utxo_info(self, wallet, address):
        utxos = wallet.listunspent(0, 10000, [address])
        utxo_info:list[tx_data] = []
        for utxo in utxos:
            raw_tx = wallet.gettransaction(utxo['txid'], True)
            dec_raw_tx = wallet.decoderawtransaction(raw_tx['hex'])
            utxo_info.append(tx_data(dec_raw_tx['vout'][0]['value'], dec_raw_tx['vout'][0], dec_raw_tx['txid']))
        print("\n-------UTXO------")
        print(utxo_info)
        print("\n-----------------")
        return utxo_info

    def run_test(self):
        node = self.nodes[0]
        self.generate(node, 1)  # Leave IBD for sethdseed
        print(f"-- 1 - {node.chain}")

        # Create and load the wallet
        self.nodes[0].createwallet(wallet_name='w0', descriptors=True)
        self.nodes[0].createwallet(wallet_name='w1', descriptors=True)
        self.nodes[0].setnetworkactive(True)
        w1 = node.get_wallet_rpc('w1')
        address1 = w1.getnewaddress()
        w0 = node.get_wallet_rpc('w0')
        address0 = w0.getnewaddress()

        # Get a new address and check balance
        self.generatetoaddress(node, 40, address0)
        print(f"BALANCE - {w0.getbalance()}")

        # Example Groth16 proof (replace with actual proof data)
        proof = b'your_groth16_proof_here'
        
        vouts = self.get_utxo_info(w0, address0)

        # Add the OP_CHECKGROTH16VERIFY script with the Groth16 proof
        tx_script = f"{OP_CHECKGROTH16VERIFY} {proof.hex()}"


        # Создаем транзакцию
        # Modify the vout to include the Groth16 proof in the scriptPubKey
        # decoded_tx['vout'][0]['scriptPubKey']['asm'] = tx_script
        # vouts[0].vout['scriptPubkey']['asm'] = tx_script
        # Recreate the transaction with the modified script
        modified_tx = w0.createrawtransaction([{
            'txid': vouts[0].txid,
            'vout': 0,
        }], {address1: 1.99})
        print()
        
        decode_modified_tx = w0.decoderawtransaction(modified_tx)
        decode_modified_tx['vin'][0]['scriptSig']['asm'] = tx_script
        print(f"\nmodified_tx - {modified_tx}\n")
        print(f"decode modified - {btfl_json(decode_modified_tx)}")

        # Sign the modified transaction
        signed_tx = w0.signrawtransactionwithwallet(decode_modified_tx)

        decode_signed_tx_tx = w0.decoderawtransaction(signed_tx['hex'])
        
        print(f"\ndecode_signed_tx_tx - {btfl_json(decode_signed_tx_tx)}")

        print(f"\nsigned_tx - {btfl_json(signed_tx)}\n")
        # Broadcast the transaction to the network
        txid = w0.sendrawtransaction(signed_tx['hex'])

        # Verify the transaction was included in the blockchain
        tx_info = w0.gettransaction(txid)
        print(f"Transaction ID: {txid}")
        assert_equal(tx_info['confirmations'], 0)
        self.generate(node, 1)  # Leave IBD for sethdseed
        tx_info = w0.gettransaction(txid)
        assert_equal(tx_info['confirmations'], 1)
        assert_equal(tx_info['confirmations'], 100)


if __name__ == '__main__':
    Groth16VerifyTest().main()