// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <primitives/block.h>

#include <hash.h>
#include <tinyformat.h>
#include <crypto/common.h>
#include <crypto/scrypt.h>
#include <atomic>

#define BEGIN(a)            ((char*)&(a))
#define END(a)              ((char*)&((&(a))[1]))

std::atomic<int> global_height(0);

int GetGlobHeight() 
{
    return global_height.load(std::memory_order_relaxed);
}

void SetGlobHeight(int new_height) 
{
    global_height.store(new_height, std::memory_order_relaxed);
}

void CBlockHeader::SetAuxpow (std::unique_ptr<CAuxPow> apow)
{
    if (apow != nullptr)
    {
        auxpow.reset(apow.release());
        SetAuxpowVersion(true);
    } else
    {
        auxpow.reset();
        SetAuxpowVersion(false);
    }
}

std::string CBlock::ToString() const
{
    std::stringstream s;
    s << strprintf("CBlock(hash=%s, ver=0x%08x, hashPrevBlock=%s, hashMerkleRoot=%s, nTime=%u, nBits=%08x, nNonce=%u, vtx=%u)\n",
        GetHash().ToString(),
        nVersion,
        hashPrevBlock.ToString(),
        hashMerkleRoot.ToString(),
        nTime, nBits, nNonce,
        vtx.size());
    for (const auto& tx : vtx) {
        s << "  " << tx->ToString() << "\n";
    }
    return s.str();
}
