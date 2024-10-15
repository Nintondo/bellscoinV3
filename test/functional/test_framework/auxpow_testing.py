#!/usr/bin/env python3
# Copyright (c) 2014-2019 Daniel Kraft
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

# Utility routines for auxpow that are needed specifically by the regtests.
# This is mostly about actually *solving* an auxpow block (with regtest
# difficulty) or inspecting the information for verification.

# This module requires a built and installed version of the ltc_scrypt
# package, which can be downloaded from:
# https://pypi.python.org/packages/source/l/ltc_scrypt/ltc_scrypt-1.0.tar.gz
# python3 setup.py install

import binascii
import ltc_scrypt

from test_framework import auxpow

def computeAuxpow (block, target, ok):
  """
  Build an auxpow object (serialised as hex string) that solves
  (ok = True) or doesn't solve (ok = False) the block.
  """

  (tx, header) = auxpow.constructAuxpow (block)
  (header, _) = mineBlock2 (header, target, ok)
  return auxpow.finishAuxpow (tx, header)

def mineAuxpowBlock (node, wallet):
  """
  Mine an auxpow block on the given RPC connection.  This uses the
  createauxblock and submitauxblock command pair.
  """

  def create ():
    if wallet is None:
      addr = node.getnewaddress ()
    else:
      addr = wallet.get_address()
    return node.createauxblock (addr)

  return mineAuxpowBlockWithMethods (create, node.submitauxblock)

def mineAuxpowBlockWithMethods (create, submit):
  """
  Mine an auxpow block, using the given methods for creation and submission.
  """
  auxblock = create ()
  target = auxpow.reverseHex (auxblock['_target'])
  apow = computeAuxpow (auxblock['hash'], target, True)
  res = submit (auxblock['hash'], apow)
  assert res

  return auxblock['hash']

def getCoinbaseAddr (node, blockHash):
    """
    Extract the coinbase tx' payout address for the given block.
    """

    blockData = node.getblock (blockHash)
    txn = blockData['tx']
    assert len (txn) >= 1

    txData = node.getrawtransaction (txn[0], True, blockHash)
    assert len (txData['vout']) >= 1 and len (txData['vin']) == 1
    assert 'coinbase' in txData['vin'][0]

    addr = txData['vout'][0]['scriptPubKey']['address']
    assert len (addr) > 0
    return addr

def mineBlock (header, target, ok):
  """
  Given a block header, update the nonce until it is ok (or not)
  for the given target.
  """

  data = bytearray (binascii.unhexlify (header))
  while True:
    assert data[79] < 255
    data[79] += 1
    hexData = binascii.hexlify (data)

    blockhash = auxpow.doubleHashHex (hexData)
    if (ok and blockhash < target) or ((not ok) and blockhash > target):
      break

  return (hexData, blockhash)

def mineBlock2 (header, target, ok):
  """
  Given a block header, update the nonce until it is ok (or not)
  for the given target.
  """

  data = bytearray (binascii.unhexlify(header))
  while True:
    assert data[79] < 255
    data[79] += 1
    hexData = binascii.hexlify(data)

    scrypt = getScryptPoW(hexData)
    if (ok and scrypt < target) or ((not ok) and scrypt > target):
      break

  blockhash = auxpow.doubleHashHex (hexData)
  return (hexData, blockhash)

# for now, just offer hashes to rpc until it matches the work we need
def mineScryptAux (node, ok):
  """
  Mine an auxpow block on the given RPC connection.
  """

  auxblock = node.getauxblock ()
  target =  auxpow.reverseHex (auxblock['_target'])

  apow = computeAuxpow (auxblock['hash'], target, ok)
  res = node.getauxblock (auxblock['hash'], apow)
  return res

def mineScryptBlock (header, target, ok):
  """
  Given a block header, update the nonce until it is ok (or not)
  for the given target.
  """

  data = bytearray (binascii.unhexlify(header))
  while True:
    assert data[79] < 255
    data[79] += 1
    hexData = binascii.hexlify(data).decode("ascii")

    scrypt = getScryptPoW(hexData)
    if (ok and scrypt < target) or ((not ok) and scrypt > target):
      break

  blockhash = auxpow.doubleHashHex (hexData)
  return (hexData, blockhash)

def getScryptPoW(hexData):
  """
  Actual scrypt pow calculation
  """

  data = binascii.unhexlify(hexData)

  return  auxpow.reverseHex(binascii.hexlify(ltc_scrypt.getPoWHash(data)))