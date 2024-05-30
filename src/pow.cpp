// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pow.h>

#include <chain.h>
#include <primitives/block.h>
#include <uint256.h>
#include <iostream>
#include <logging.h>

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, 
                                    const CBlockHeader *pblock, 
                                    const Consensus::Params& params)
{
    if (pindexLast->nHeight <= params.nNewPowDiffHeight)
        return GetNextWorkRequiredOld(pindexLast, pblock, params);
    else
        return GetNextWorkRequiredNew(pindexLast, pblock, params);
}

unsigned int CalculateNextWorkRequired(
                                    const Consensus::Params& params,
                                    int64_t nFirstBlockTime, 
                                    const CBlockIndex* pindexLast, 
                                    arith_uint256 bnAvg,
                                    int64_t nLastBlockTime,
                                    int nextHeight)
{
    if (pindexLast->nHeight <= params.nNewPowDiffHeight)
        return CalculateNextWorkRequiredOld(pindexLast, nFirstBlockTime, params);
    else
        return CalculateNextWorkRequiredNew(bnAvg, nLastBlockTime, nFirstBlockTime, params);
}

unsigned int GetNextWorkRequiredOld(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    assert(pindexLast != nullptr);
    unsigned int nProofOfWorkLimit = UintToArith256(params.powLimit).GetCompact();
    
    // Genesis block
    if (pindexLast == NULL)
        return nProofOfWorkLimit;

    // Only change once per difficulty adjustment interval
    if ((pindexLast->nHeight+1) % params.DifficultyAdjustmentInterval() != 0)
    {
        if (params.fPowAllowMinDifficultyBlocks)
        {
            // Special difficulty rule for testnet:
            // If the new block's timestamp is more than 4 minutes
            // then allow mining of a min-difficulty block.
            if (pblock->GetBlockTime() > pindexLast->GetBlockTime() + params.PoWTargetSpacing().count()*4)
            {
                return nProofOfWorkLimit;
            }
            else
            {
                const CBlockIndex* pindex = pindexLast;

                while (pindex->pprev && pindex->nHeight % params.DifficultyAdjustmentInterval() != 0 && pindex->nBits == nProofOfWorkLimit)
                    pindex = pindex->pprev;
                return pindex->nBits;
            }
        }
        return pindexLast->nBits;
    }

    // Go back by what we want to be 14 days worth of blocks

    // Litecoin: This fixes an issue where a 51% attack can change difficulty at will.
    // Go back the full period unless it's the first retarget after genesis. Code courtesy of Art Forz

    int blockstogoback = params.DifficultyAdjustmentInterval()-1;
    if ((pindexLast->nHeight+1) != params.DifficultyAdjustmentInterval())
        blockstogoback = params.DifficultyAdjustmentInterval();

    // Original code
    // ---
    // Go back by what we want to be 14 days worth of blocks
    // const CBlockIndex* pindexFirst = pindexLast;
    // for (int i = 0; pindexFirst && i < blockstogoback; i++)
    //    pindexFirst = pindexFirst->pprev;
    //
    // assert(pindexFirst);
    // ---
    

    int nHeightFirst = pindexLast->nHeight - blockstogoback;
    assert(nHeightFirst >= 0);
    const CBlockIndex* pindexFirst = pindexLast->GetAncestor(nHeightFirst);
    assert(pindexFirst);

    return CalculateNextWorkRequiredOld(pindexLast, pindexFirst->GetBlockTime(), params);
}

unsigned int GetNextWorkRequiredNew(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    unsigned int nProofOfWorkLimit = UintToArith256(params.powLimit).GetCompact();

    // Genesis block
    if (pindexLast == NULL)
    {
        LogPrintf("------> Genesis block. Return nProofOfWorkLimit - %d\n", nProofOfWorkLimit);
        return nProofOfWorkLimit;
    }

    // Regtest
    if (params.fPowNoRetargeting)
    {
        LogPrintf("------> Return params.fPowNoRetargeting - %d\n", pindexLast->nBits);
        return pindexLast->nBits;
    }

    {
        // Comparing to pindexLast->nHeight with >= because this function
        // returns the work required for the block after pindexLast.
        if (params.nPowAllowMinDifficultyBlocksAfterHeight != std::nullopt &&
            pindexLast->nHeight >= params.nPowAllowMinDifficultyBlocksAfterHeight.value())
        {
            // Special difficulty rule for testnet:
            // If the new block's timestamp is more than 6 * block interval minutes
            // then allow mining of a min-difficulty block.
            if (pblock && pblock->GetBlockTime() > pindexLast->GetBlockTime() + params.PoWTargetSpacing().count() * 6)
            {
                LogPrintf("------> Testnet Return nProofOfWorkLimit - %d\n", nProofOfWorkLimit);
                return nProofOfWorkLimit;
            }
        }
    }

    // Find the first block in the averaging interval
    const CBlockIndex* pindexFirst = pindexLast;
    arith_uint256 bnTot {0};
    for (int i = 0; pindexFirst && i < params.nPowAveragingWindow; i++) {
        arith_uint256 bnTmp;
        bnTmp.SetCompact(pindexFirst->nBits);
        bnTot += bnTmp;
        pindexFirst = pindexFirst->pprev;
    }

    // Check we have enough blocks
    if (pindexFirst == NULL)
    {
        LogPrintf("------> We have enough blocks Return nProofOfWorkLimit - %d\n", nProofOfWorkLimit);
        return nProofOfWorkLimit;
    }

    // The protocol specification leaves MeanTarget(height) as a rational, and takes the floor
    // only after dividing by AveragingWindowTimespan in the computation of Threshold(height):
    // <https://zips.z.cash/protocol/protocol.pdf#diffadjustment>
    //
    // Here we take the floor of MeanTarget(height) immediately, but that is equivalent to doing
    // so only after a further division, as proven in <https://math.stackexchange.com/a/147832/185422>.
    arith_uint256 bnAvg {bnTot / params.nPowAveragingWindow};

    return CalculateNextWorkRequiredNew(bnAvg,
                                    pindexLast->GetMedianTimePast(), pindexFirst->GetMedianTimePast(),
                                    params);
}


unsigned int CalculateNextWorkRequiredNew(arith_uint256 bnAvg,
                                    int64_t nLastBlockTime, 
                                    int64_t nFirstBlockTime,
                                    const Consensus::Params& params)
{
    int64_t averagingWindowTimespan = params.AveragingWindowTimespan();
    int64_t minActualTimespan = params.MinActualTimespan();
    int64_t maxActualTimespan = params.MaxActualTimespan();
    // Limit adjustment step
    // Use medians to prevent time-warp attacks
    int64_t nActualTimespan = nLastBlockTime - nFirstBlockTime;
    nActualTimespan = averagingWindowTimespan + (nActualTimespan - averagingWindowTimespan)/4;

    if (nActualTimespan < minActualTimespan) {
        LogPrintf("------> nActualTimespan < minActualTimespan { nActualTimespan %d = %d minActualTimespan }\n", nActualTimespan, minActualTimespan);
        nActualTimespan = minActualTimespan;
    }
    if (nActualTimespan > maxActualTimespan) {
        LogPrintf("------> nActualTimespan > maxActualTimespan { nActualTimespan %d = %d maxActualTimespan }\n",nActualTimespan, maxActualTimespan);
        nActualTimespan = maxActualTimespan;
    }

    // Retarget
    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
    arith_uint256 bnNew {bnAvg};
    bnNew /= averagingWindowTimespan;
    bnNew *= nActualTimespan;

    if (bnNew > bnPowLimit) {
        LogPrintf("------> bnNew > bnPowLimit { bnNew %d = %d bnPowLimit }\n", bnNew.GetCompact(), bnPowLimit.GetCompact());
        LogPrintf("------> bnNew > bnPowLimit { bnNew %s = %s bnPowLimit }\n", bnNew.ToString(), bnPowLimit.ToString());
        bnNew = bnPowLimit;
    }
    
    LogPrintf("------> CalculateNextWorkRequiredNew Return bnNew.GetCompact() - %d\n", bnNew.GetCompact());
    LogPrintf("------> CalculateNextWorkRequiredNew Return bnNew.ToString() - %s\n", bnNew.ToString());
    return bnNew.GetCompact();
}

unsigned int CalculateNextWorkRequiredOld(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params& params)
{

    int nHeight = pindexLast->nHeight + 1;
    const int64_t retargetTimespan = params.nPowTargetTimespan;
    const int64_t nActualTimespan = pindexLast->GetBlockTime() - nFirstBlockTime;
    int64_t nModulatedTimespan = nActualTimespan;
    int64_t nMaxTimespan;
    int64_t nMinTimespan;

    if (nHeight > 10000) {
        nMinTimespan = retargetTimespan / 4;
        nMaxTimespan = retargetTimespan * 4;
    } else if (nHeight > 5000) {
        nMinTimespan = retargetTimespan / 8;
        nMaxTimespan = retargetTimespan * 4;
    } else {
        nMinTimespan = retargetTimespan / 16;
        nMaxTimespan = retargetTimespan * 4;
    }

    // Limit adjustment step
    if (nModulatedTimespan < nMinTimespan)
        nModulatedTimespan = nMinTimespan;
    else if (nModulatedTimespan > nMaxTimespan)
        nModulatedTimespan = nMaxTimespan;

    // Retarget
    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
    arith_uint256 bnNew;
    arith_uint256 bnOld;
    bnNew.SetCompact(pindexLast->nBits);
    bnOld = bnNew;
    bnNew *= nModulatedTimespan;
    bnNew /= retargetTimespan;

    if (bnNew > bnPowLimit)
        bnNew = bnPowLimit;

    return bnNew.GetCompact();
}

// Check that on difficulty adjustments, the new difficulty does not increase
// or decrease beyond the permitted limits.
bool PermittedDifficultyTransition(const Consensus::Params& params, int64_t height, uint32_t old_nbits, uint32_t new_nbits)
{
    if (params.fPowAllowMinDifficultyBlocks)
    {
        LogPrintf("------> PermittedDifficultyTransition return true fPowAllowMinDifficultyBlocks\n");
        return true;
    } 

    if (height % params.DifficultyAdjustmentInterval() == 0) {
        int64_t smallest_timespan = params.nPowTargetTimespan/4;
        int64_t largest_timespan = params.nPowTargetTimespan*4;

        const arith_uint256 pow_limit = UintToArith256(params.powLimit);
        arith_uint256 observed_new_target;
        observed_new_target.SetCompact(new_nbits);

        // Calculate the largest difficulty value possible:
        arith_uint256 largest_difficulty_target;
        largest_difficulty_target.SetCompact(old_nbits);
        largest_difficulty_target *= largest_timespan;
        largest_difficulty_target /= params.nPowTargetTimespan;

        if (largest_difficulty_target > pow_limit) {
            largest_difficulty_target = pow_limit;
        }

        // Round and then compare this new calculated value to what is
        // observed.
        arith_uint256 maximum_new_target;
        maximum_new_target.SetCompact(largest_difficulty_target.GetCompact());
        if (maximum_new_target < observed_new_target)
        {
            LogPrintf("------> PermittedDifficultyTransition return false\n");
            return false;
        } 
        // Calculate the smallest difficulty value possible:
        arith_uint256 smallest_difficulty_target;
        smallest_difficulty_target.SetCompact(old_nbits);
        smallest_difficulty_target *= smallest_timespan;
        smallest_difficulty_target /= params.nPowTargetTimespan;

        if (smallest_difficulty_target > pow_limit) {
            smallest_difficulty_target = pow_limit;
        }

        // Round and then compare this new calculated value to what is
        // observed.
        arith_uint256 minimum_new_target;
        minimum_new_target.SetCompact(smallest_difficulty_target.GetCompact());
        if (minimum_new_target > observed_new_target)
        {
            LogPrintf("------> PermittedDifficultyTransition minimum_new_target > observed_new_target return false\n");
            return false;
        }
    } else if (old_nbits != new_nbits) {
        LogPrintf("------> PermittedDifficultyTransition old_nbits != new_nbits return true\n");
        return false;
    }
    LogPrintf("------> PermittedDifficultyTransition return true\n");
    return true;
}

bool CheckProofOfWork(uint256 hash, unsigned int nBits, const Consensus::Params& params)
{
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;

    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);
    
    // Check range
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(params.powLimit))
    {
        //printf("123123 bntarget %s > powlimit %s \n", bnTarget.ToString().c_str(), params.powLimit.ToString().c_str());
        return false;
    }

    // Check proof of work matches claimed amount
    if (UintToArith256(hash) > bnTarget)
    {
        //printf("5123123 hash %s > powlimit %s \n", hash.ToString().c_str(), bnTarget.ToString().c_str());
        return false;
    }

    return true;
}

bool CheckProofOfWorkTests(uint256 hash, unsigned int nBits, const Consensus::Params& params, bool wtf)
{
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;

    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);
    
    // Check range
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(params.powLimit))
    {
        return false;
    }

    // Check proof of work matches claimed amount
    if (UintToArith256(hash) > bnTarget)
    {
        return false;
    }

    return true;
}
