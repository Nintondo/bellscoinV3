// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#include "params.h"

#include <chainparams.h>
#include <deploymentstatus.h>

namespace Consensus {
    
    std::chrono::seconds Params::PoWTargetSpacing(bool tests) const {
        //std::chrono::seconds tmp = tests ? std::chrono::seconds{nPostBlossomPowTargetSpacing} : std::chrono::seconds{10*60};
    
        //return tests ? std::chrono::seconds{nPostBlossomPowTargetSpacing} : std::chrono::seconds{10*60}; // Old
        
        return std::chrono::seconds{nPostBlossomPowTargetSpacing}; // New
    }

    int64_t Params::AveragingWindowTimespan() const {
        return nPowAveragingWindow * PoWTargetSpacing().count();
    }

    int64_t Params::MinActualTimespan() const {
        return (AveragingWindowTimespan() * (100 - nPowMaxAdjustUp)) / 100;
    }

    int64_t Params::MaxActualTimespan() const {
        return (AveragingWindowTimespan() * (100 + nPowMaxAdjustDown)) / 100;
    }
    
};
