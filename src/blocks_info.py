import argparse
import requests
#example of use: python3 blocks_info.py <from wich height> <how much block analyze from 1 arg>
# python3 blocks_info.py 250 100
# Total blocks: 381
# Get 100 blocks; 
# Stat for blocks from 250 to 350:
# - Total time (min:sec): 84:24
# - Blocks per minute: 1.184834

class BlockInfo:
    def __init__(self, height, time, median_time):
        self.height = height
        self.time = time
        self.median_time = median_time
    

parser = argparse.ArgumentParser(description='A simple script that adds two numbers.')

# Add the arguments
parser.add_argument('from_block_height', type=int, help='From wich block start calc')
parser.add_argument('block_amount', type=int, help='How much block need calc')
# Parse the arguments
args = parser.parse_args()

url = "http://127.0.0.1:18332/"
headers = {
    "content-type": "application/json",
}

data = '{"jsonrpc": "1.0", "id": "1", "method": "getblockcount", "params": []}'
response = requests.post(url, headers=headers, data=data, auth=("yourusername", "yourpassword"))
block_count=response.json()['result']
data='{"jsonrpc": "1.0", "id": "2", "method": "getblockhash", "params": [%d] }' % args.from_block_height
block_hash = requests.post(url, headers=headers, data=data, auth=("yourusername", "yourpassword")).json()['result']

blocks = []
last_block_hash = block_hash
for i in range(args.block_amount):
    data = '{"jsonrpc": "1.0", "id": "3", "method": "getblock", "params": ["%s"] }' % last_block_hash
    block = requests.post(url, headers=headers, data=data, auth=("yourusername", "yourpassword")).json()
    tmp_blk = BlockInfo(block['result']['height'], block['result']['time'], block['result']['mediantime'])
    blocks.append(tmp_blk)
    last_block_hash = block['result']['nextblockhash']

    # Calculate the number of blocks produced per minute using the block time
num_blocks = len(blocks)
total_time = blocks[-1].time - blocks[0].time
minutes = (total_time / 60.0)
blocks_per_minute = num_blocks / minutes

minutes, seconds = divmod(total_time, 60)
# Format the total time as a string in the format "mm:ss"
total_time_str = "{:02d}:{:02d}".format(minutes, seconds)
print("Total blocks: %d" % block_count)
print("Get %d blocks; \nStat for blocks from %d to %d:" % (num_blocks, blocks[0].height, blocks[-1].height+1))
print("- Total time (min:sec): %s" % total_time_str)
print("- Blocks per minute: %f" % blocks_per_minute)