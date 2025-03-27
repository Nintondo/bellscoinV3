// Copyright (c) 2018-2023 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <cstdint>
#include <optional>

class CBlockIndex;

namespace Consensus {
struct Params;
}

/** Check if protocol upgrade has activated. */
bool IsUpgrade8Enabled(const Consensus::Params &params, const CBlockIndex *pindexPrev);
