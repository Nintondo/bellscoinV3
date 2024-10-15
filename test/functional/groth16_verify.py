#!/usr/bin/env python3
# Copyright (c) 2014-2024 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test (OP_CHECKGROTH16VERIFY)
"""

from test_framework.blocktools import (
    create_coinbase,
    create_block,
    add_witness_commitment,
)

from test_framework.messages import (
    CTransaction,
    CTxOut,
    CTxIn,
    CTxInWitness,
    COutPoint,
    COIN,
    sha256
)
from test_framework.address import (
    hash160,
)
from test_framework.p2p import P2PInterface
from test_framework.script import (
    CScript,
    OP_CHECKGROTH16VERIFY,
    OP_HASH160,
    OP_EQUAL,
    OP_CHECKMULTISIG,
    taproot_construct,
)
from test_framework.script_util import script_to_p2sh_script
from test_framework.key import ECKey, compute_xonly_pubkey
from test_framework.test_framework import BellscoinTestFramework
from test_framework.util import assert_equal, assert_raises_rpc_error
from test_framework.wallet import MiniWallet, MiniWalletMode
from decimal import Decimal
import random
from io import BytesIO
from test_framework.address import script_to_p2sh

DISABLED_OP_CODE = (
    "mandatory-script-verify-flag-failed (Attempted to use a disabled opcode)"
)
BAD_STACKSIZE_OP_CODE = (
    "mandatory-script-verify-flag-failed (Operation not valid with the current stack size)"
)

def random_bytes(n):
    return bytes(random.getrandbits(8) for i in range(n))


def random_p2sh():
    return script_to_p2sh_script(random_bytes(20))


def create_transaction_to_script(node, wallet, txid, script, *, amount_sats):
    """Return signed transaction spending the first output of the
    input txid. Note that the node must be able to sign for the
    output that is being spent, and the node must not be running
    multiple wallets.
    """
    random_address = script_to_p2sh(CScript())
    output = wallet.get_utxo(txid=txid)
    rawtx = node.createrawtransaction(
        inputs=[{"txid": output["txid"], "vout": output["vout"]}],
        outputs={random_address: Decimal(amount_sats) / COIN},
    )
    tx = CTransaction()
    tx.deserialize(BytesIO(bytes.fromhex(rawtx)))
    # Replace with our script
    tx.vout[0].scriptPubKey = script
    # Sign
    wallet.sign_tx(tx)
    return tx

def print_transaction_details(tx):
    print("Transaction Inputs:")
    for vin in tx.vin:
        print(f"- Txid: {vin.prevout.hash}, Vout: {vin.prevout.n}, Sequence: {vin.nSequence}")

    print("Transaction Outputs:")
    for vout in tx.vout:
        print(f"- Value: {vout.nValue}, ScriptPubKey: {vout.scriptPubKey.hex()}")

class GrothTest(BellscoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.extra_args = [
            ["-par=1"]
        ]  # Use only one script thread to get the exact reject reason for testing
        self.setup_clean_chain = True
        self.rpc_timeout = 120

    def get_block(self, txs):
        self.tip = self.nodes[0].getbestblockhash()
        self.height = self.nodes[0].getblockcount()
        self.log.debug(self.height)
        block = create_block(
            int(self.tip, 16), create_coinbase(self.height + 1))
        block.vtx.extend(txs)
        add_witness_commitment(block)
        block.hashMerkleRoot = block.calc_merkle_root()
        block.solve()
        return block.serialize(True).hex(), block.hash

    def add_block(self, txs):
        block, h = self.get_block(txs)
        reason = self.nodes[0].submitblock(block)
        if reason:
            self.log.debug("Reject Reason: [%s]", reason)
        assert_equal(self.nodes[0].getbestblockhash(), h)
        return h
    
    def run_test(self):
        # The goal of this test suite is to rest OP_CHECKGROTH16VERIFY is disabled by default in segwitv0 and p2sh script.

        wallet = MiniWallet(self.nodes[0], mode=MiniWalletMode.RAW_P2PK)
        self.nodes[0].add_p2p_connection(P2PInterface())

        BLOCKS = 500
        self.log.info("Mining %d blocks for mature coinbases", BLOCKS)
        # Drop the last 100 as they're unspendable!
        coinbase_txids = [
            self.nodes[0].getblock(b)["tx"][0]
            for b in self.generate(wallet, BLOCKS)[:-100]
        ]
        def get_coinbase(): return coinbase_txids.pop()
        self.log.info("Creating setup transactions")

        outputs = [CTxOut(1000 + i*10, random_p2sh()) for i in range(1, 21)]

        print("Transaction Outputs:")
        for i, output in enumerate(outputs):
            print(f"- Output {i}: Value: {output.nValue}, ScriptPubKey: {output.scriptPubKey.hex()}")

        # Add some fee
        amount_sats = sum(out.nValue for out in outputs) + 200 * 500
        print(f" amount sats = {amount_sats}")
        
        private_key = ECKey()
        # use simple deterministic private key (k=1)
        private_key.set((1).to_bytes(32, "big"), False)
        assert private_key.is_valid
        public_key, _ = compute_xonly_pubkey(private_key.get_bytes())

        op_groth16_script = CScript([
            # Calling OP_CHECKGROTH16VERIFY on an empty stack
            # The content of the stack doesn't really matter for what we are testing
            # The interpreter should never get to the point where its executing this OP_CHECKGROTH16VERIFY instruction
            OP_CHECKGROTH16VERIFY,
        ])

        self.log.info("Creating a OP_CHECKGROTH16VERIFY tapscript funding tx")
        taproot_op_groth16 = taproot_construct(
            public_key, [("only-path", op_groth16_script, 0xC0)])
        taproot_op_groth16_funding_tx = create_transaction_to_script(
            self.nodes[0],
            wallet,
            get_coinbase(),
            taproot_op_groth16.scriptPubKey,
            amount_sats=amount_sats,
        )

        self.log.info("Creating a OP_CHECKGROTH16VERIFY segwit funding tx")
        segwit_groth_funding_tx = create_transaction_to_script(
            self.nodes[0],
            wallet,
            get_coinbase(),
            CScript([0, sha256(op_groth16_script)]),
            amount_sats=amount_sats,
        )

        self.log.info("Create p2sh OP_CHECKGROTH16VERIFY funding tx")
        p2sh_groth_funding_tx = create_transaction_to_script(
            self.nodes[0],
            wallet,
            get_coinbase(),
            CScript([OP_HASH160, hash160(op_groth16_script), OP_EQUAL]),
            amount_sats=amount_sats,
        )

        self.log.info("Create p2sh OP_CHECKGROTH16VERIFY empty funding tx")
        p2sh_groth_empty_funding_tx = create_transaction_to_script(
            self.nodes[0],
            wallet,
            get_coinbase(),
            CScript([b'', OP_CHECKGROTH16VERIFY]),
            amount_sats=amount_sats,
        )
        
        print_transaction_details(segwit_groth_funding_tx)
        print_transaction_details(taproot_op_groth16_funding_tx)
        print_transaction_details(p2sh_groth_funding_tx)
        print_transaction_details(p2sh_groth_empty_funding_tx)

        funding_txs = [
            taproot_op_groth16_funding_tx,
            segwit_groth_funding_tx,
            p2sh_groth_funding_tx,
            p2sh_groth_empty_funding_tx
        ]
        (
            taproot_op_groth16_outpoint,
            segwit_op_groth16_outpoint,
            bare_op_groth16_outpoint,
            p2sh_groth_empty_funding_tx
        ) = [COutPoint(int(tx.rehash(), 16), 0) for tx in funding_txs]

        self.log.info("Funding all outputs")
        self.add_block(funding_txs)
        self.log.info("END Funding all outputs")
        
        taproot_op_groth16_transaction = CTransaction()
        taproot_op_groth16_transaction.vin = [
            CTxIn(taproot_op_groth16_outpoint)]
        taproot_op_groth16_transaction.vout = outputs
        taproot_op_groth16_transaction.wit.vtxinwit += [
            CTxInWitness()]
        taproot_op_groth16_transaction.wit.vtxinwit[0].scriptWitness.stack = [
            op_groth16_script,
            bytes([0xC0 + taproot_op_groth16.negflag]) +
            taproot_op_groth16.internal_pubkey,
        ]

        assert_raises_rpc_error(
            -26,
            BAD_STACKSIZE_OP_CODE,
            self.nodes[0].sendrawtransaction,
            taproot_op_groth16_transaction.serialize().hex(),
        )
        self.log.info("Taproot OP_CHECKGROTH16VERIFY spend failed with expected error")

        self.log.info("Testing OP_CHECKGROTH16VERIFY with invalid input")

        invalid_groth16_tx = CTransaction()
        invalid_groth16_tx.vin = [CTxIn(p2sh_groth_empty_funding_tx)]
        invalid_groth16_tx.vin[0].scriptSig = CScript([b'', OP_CHECKGROTH16VERIFY])
        invalid_groth16_tx.vout = outputs

        assert_raises_rpc_error(
            -26,
            BAD_STACKSIZE_OP_CODE,
            self.nodes[0].sendrawtransaction,
            invalid_groth16_tx.serialize().hex(),
        )

        invalid_groth16_script = CScript([b'', OP_CHECKGROTH16VERIFY])
        
        invalid_groth16_tx = create_transaction_to_script(
            self.nodes[0],
            wallet,
            get_coinbase(),
            CScript([OP_HASH160, hash160(invalid_groth16_script), OP_EQUAL]),
            amount_sats=int(10000),
        )

        print_transaction_details(invalid_groth16_tx)

        assert_raises_rpc_error(
            -26,
            DISABLED_OP_CODE,
            self.nodes[0].sendrawtransaction,
            invalid_groth16_tx.serialize().hex(),
        )

        self.log.info("OP_CHECKGROTH16VERIFY with invalid input failed with expected error")

        self.log.info("Testing OP_CHECKGROTH16VERIFY with multisig")

        multisig_script = CScript([2, public_key, public_key2, 2, OP_CHECKMULTISIG])
        groth_multisig_tx = create_transaction_to_script(
            self.nodes[0],
            wallet,
            get_coinbase(),
            CScript([multisig_script, OP_CHECKGROTH16VERIFY]),
            amount_sats=int(10000),
        )

        assert_raises_rpc_error(
            -26,
            DISABLED_OP_CODE,
            self.nodes[0].sendrawtransaction,
            groth_multisig_tx.serialize().hex(),
        )
        self.log.info("OP_CHECKGROTH16VERIFY with multisig failed with expected error")

        self.log.info("Testing Segwitv0 OP_CHECKGROTH16VERIFY usage is disabled")
        segwitv0_op_groth16_transaction = CTransaction()
        segwitv0_op_groth16_transaction.vin = [
            CTxIn(segwit_op_groth16_outpoint)]
        segwitv0_op_groth16_transaction.vout = outputs
        segwitv0_op_groth16_transaction.wit.vtxinwit += [
            CTxInWitness()]
        segwitv0_op_groth16_transaction.wit.vtxinwit[0].scriptWitness.stack = [
            op_groth16_script,
        ]

        assert_raises_rpc_error(
            -26,
            BAD_STACKSIZE_OP_CODE,
            self.nodes[0].sendrawtransaction,
            segwitv0_op_groth16_transaction.serialize().hex(),
        )
        self.log.info("Segwitv0 OP_CHECKGROTH16VERIFY spend failed with expected error")

        self.log.info("Testing p2sh script OP_CHECKGROTH16VERIFY usage is disabled")
        p2sh_op_groth16_transaction = CTransaction()
        p2sh_op_groth16_transaction.vin = [
            CTxIn(bare_op_groth16_outpoint)]
        p2sh_op_groth16_transaction.vin[0].scriptSig = CScript(
            [op_groth16_script])
        p2sh_op_groth16_transaction.vout = outputs

        assert_raises_rpc_error(
            -26,
            BAD_STACKSIZE_OP_CODE,
            self.nodes[0].sendrawtransaction,
            p2sh_op_groth16_transaction.serialize().hex(),
        )
        self.log.info("p2sh OP_CHECKGROTH16VERIFY spend failed with expected error")


if __name__ == "__main__":
    print(f"Path: {__file__}")
    GrothTest().main()