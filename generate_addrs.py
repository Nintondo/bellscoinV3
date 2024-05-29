import subprocess
import time
from datetime import datetime

class AddressKeyPair:
    def __init__(self, address, privkey):
        self.address = address
        self.privkey = privkey

PRIV_KEYS = []

def run_command(command):
    """Run command in terminal"""
    result = subprocess.run(command, shell=True, text=True, capture_output=True)
    return result.stdout.strip()

    #wallet amount 
for i in range(12):
    run_command("rm -rf /home/dmatsiukhov/.bells/regtest/wallets/*")
    
    bitcoind_process = subprocess.Popen(["./src/bitcoind", "-regtest", "-rpcport=18443", "-deprecatedrpc=create_bdb"])
    
    time.sleep(2)
    
    # Create wallet
    run_command("./src/bitcoin-cli -regtest -rpcport=18443 createwallet \"legacy_wallet1\" false false \"\" false false true")
    
    # Generate addr
    new_address = run_command("./src/bitcoin-cli -regtest -rpcport=18443 getnewaddress")
    print(f"New address: {new_address}")
    
    # Dump privkey
    priv_key = run_command(f"./src/bitcoin-cli -regtest -rpcport=18443 dumpprivkey \"{new_address}\"")
    print(f"Private key: {priv_key}")
    
    PRIV_KEYS.append(AddressKeyPair(new_address, priv_key))
    bitcoind_process.terminate()
    bitcoind_process.wait()
    time.sleep(1)

filename = "all_wallets_info.txt"
with open(filename, "w") as file:
    for pair in PRIV_KEYS:
        file.write(f"Address: {pair.address}, PrivKey: {pair.privkey}\n")
