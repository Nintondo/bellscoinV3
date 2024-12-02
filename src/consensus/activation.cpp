// Copyright (c) 2018-2023 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/activation.h>

#include <chain.h>
#include <consensus/params.h>


bool IsUpgrade8Enabled(const Consensus::Params &params, const CBlockIndex *pindexPrev) {
    if (pindexPrev == nullptr) {
        return false;
    }

    return pindexPrev->nHeight >= params.upgrade8Height;
}
