<h1 align="center">
<img src="https://bellscoin.com/assets/bell-bag-1x-300-CRltNSoe.webp" data-canonical-src="https://bellscoin.com/assets/bell-bag-1x-300-CRltNSoe.webp" width="301" height="360" alt="Bells"/>
<br/><br/>
Bells Core [BEL]
</h1>

<div align="center">

[![BellsBadge](https://img.shields.io/badge/Bells-Coin-blue)](https://bellscoin.com)
[![MuchWow](https://img.shields.io/badge/OG-Coin-yellow.svg)](https://bellscoin.com)

</div>

## This branch contains the latest version 3.0.0 of the bells network.

## What is Bells?
Bells is the twin of DogeCoin, born before it, and just seen the lights now. [Visit blockchain explorer](https://nintondo.io/explorer)

## Development and contributions
Developers work in their own trees, then submit pull requests when they think
their feature or bug fix is ready.

## Usage 💻

To start your journey with Bells Core, see the [installation guide](INSTALL.md) and the [getting started](doc/getting-started.md) tutorial.

The JSON-RPC API provided by Bells Core is self-documenting and can be browsed with `bells-cli help`, while detailed information for each command can be viewed with `bells-cli help <command>`. Alternatively, see the [Bitcoin Core documentation](https://developer.bitcoin.org/reference/rpc/) - which implement a similar protocol - to get a browsable version.

## Frequently Asked Questions

### How much doge can exist?
Total of 500,000,000 coins, special rewards system.

### How get $bel?
Scrypt Proof of Work

1 Minute Block Targets, 4 Hour Diff Readjustments

* 50% chance of 50 coins
* 20% chance of 100 coins
* 14% chance of 250 coins
* 10% chance of 500 coins
* 5% chance of 1000 coins
* 1% chance of 10000 coins

Halving at 129600 (~90 days)
Decreasing by 4/5ths at 259200 blocks (~180 days)
After block 518,400 (~1 year), reward of 2 coins.

### Ports
| Function | mainnet | testnet | regtest |
| :------- |--------:| ------: | ------: |
| P2P      |   19919 |   29919 |   18444 |
| RPC      |   19918 |   29929 |   18332 |

## Change Log
- Added auxpow merge mining with Scrypt and mining stuff
- Pchmessage change for resolve conflict with dogecoin
- Change difficulty calculation, take it from Zcash
- Fix tests, QT, and other bugs

## License ⚖️
Bells Core is released under the terms of the MIT license. See
[COPYING](COPYING) for more information or see
[opensource.org](https://opensource.org/licenses/MIT)