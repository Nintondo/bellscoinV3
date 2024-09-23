from test_framework.test_framework import BellscoinTestFramework
from test_framework.util import assert_equal

class Groth16VerifyTest(BellscoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1

    def run_test(self):
        w1 = self.nodes[0].get_wallet_rpc('w1')
        assert_equal(w1.getbalance(), 0.0)

        address1 = w1.getnewaddress()


        tx1 = self.nodes[0].createrawtransaction([], [{address1: 5.0}])
        print(tx1)
        tx = self.nodes[0].createrawtransaction([], {address1: 1})
        script = "OP_CHECKGROTH16VERIFY"

        signed_tx = self.nodes[0].signrawtransactionwithwallet(tx)
        txid = self.nodes[0].sendrawtransaction(signed_tx['hex'])

        tx_info = self.nodes[0].gettransaction(txid)
        assert_equal(tx_info['confirmations'], 1)

if __name__ == '__main__':
    Groth16VerifyTest().main()