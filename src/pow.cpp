// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pow.h>

#include <arith_uint256.h>
#include <chain.h>
#include <primitives/block.h>
#include <uint256.h>

/******************************************************************************************************************************************************************
********************DIFFICULTY FORMULA - WRITTEN BY BRIAN OTIENO - Based on Weighted Weighted Harmonic Mean********************************************************
******************************************************************************************************************************************************************/
unsigned int static BermudaGravityPull(const CBlockIndex* pindexLast, const Consensus::Params& params) {
    const CBlockIndex *BlockLastSolved = pindexLast;
    const CBlockIndex *BlockReading = pindexLast;
    uint64_t PastBlocksMass = 0;
    int64_t PastRateActualSeconds = 0;
    int64_t PastRateTargetSeconds = 0;
    double PastRateAdjustmentRatio = double(0.99);
    int64_t MaxTime;
    arith_uint256 bnNew;
  
    uint64_t PastBlocksMin = (uint64_t (params.nPowTargetTimespan * 25)) / params.nPowTargetSpacing; 
    uint64_t PastBlocksMax = (uint64_t (params.nPowTargetTimespan * 70)) / params.nPowTargetSpacing; 

    if (BlockLastSolved == NULL || BlockLastSolved->nHeight == 0 || (uint64_t)BlockLastSolved->nHeight < PastBlocksMin) { return UintToArith256(params.powLimit).GetCompact(); }
		
		pindexInitial = pindexLast->pprev;
		pindexRecent = pindexLast;
		
		/* previous_max=timestamp[height - N] - where N is most recent block */
		PastRateActualSeconds = pindexRecent->GetBlockTime() - pindexInitial->GetBlockTime(); 
		
		/* In (i = height-N+1; i < height+1; i++), applying timestamp gives this; timestamp(height -N)+1 which is just (pindexInitial - pindexRecent)+1 */
    for (unsigned int i = pindexRecent - pindexInitial + 1; i < pindexRecent; i++) { 
        if (PastBlocksMax > 0 && i > PastBlocksMax) { assert("Check if value is within the WWHM limit", (PastBlocksMax > 0 && i > PastBlocksMax)); break; }
        	MaxTime = max(pindexInitial[i]->GetBlockTime(), PastRateActualSeconds); /*max_timestamp=max(timestamp[i], previous_max)*/
		/* PastRateTargetSeconds = max_timestamp - previous_max*/
		PastRateTargetSeconds = MaxTime - PastRateActualSeconds; 
		/* PastRateTargetSeconds=1 if PastRateTargetSeconds < 1 and PastRateTargetSeconds = 10*T if PastRateTargetSeconds > 10*T */
		PastRateTargetSeconds = (PastRateTargetSeconds <1)? 1 : (PastRateTargetSeconds > (10*PastRateTargetSeconds))? (10*PastRateTargetSeconds): PastRateTargetSeconds;
		/* Re-assigning PastRateActualSeconds to new and thus next PastRateActualSeconds for the next iteration: previous_max=max_timestamp */
		PastRateActualSeconds = MaxTime;
		PastBlocksMass++; 		 
    } //Iteration for the next i;
	
/* Enough restrictions, no limits need to be set. Straight to retargeting. */

   bnNew.SetCompact(pindexLast->nBits);
   
    /*next_D = previous_D*limit if next_D > previous_D*limit */ 	
	if (PastRateActualSeconds > (PastRateTargetSeconds * UintToArith256(params.powLimit))) {
        bnNew *= PastRateActualSeconds; 
    }
	/*next_D = previous_D/limit if next_D > previous_D/limit*/
	if (PastRateActualSeconds > (PastRateTargetSeconds * UintToArith256(params.powLimit))) {
        bnNew /= PastRateTargetSeconds;
    }	  
    return bnNew.GetCompact();
}

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    assert(pindexLast != nullptr);
    unsigned int nProofOfWorkLimit = UintToArith256(params.powLimit).GetCompact();

    // Only change once per difficulty adjustment interval
    if ((pindexLast->nHeight+1) % params.DifficultyAdjustmentInterval() != 0)
    {
        if (params.fPowAllowMinDifficultyBlocks)
        {
            // Special difficulty rule for testnet:
            // If the new block's timestamp is more than 2* 10 minutes
            // then allow mining of a min-difficulty block.
            if (pblock->GetBlockTime() > pindexLast->GetBlockTime() + params.nPowTargetSpacing*2)
                return nProofOfWorkLimit;
            else
            {
                // Return the last non-special-min-difficulty-rules-block
                const CBlockIndex* pindex = pindexLast;
                while (pindex->pprev && pindex->nHeight % params.DifficultyAdjustmentInterval() != 0 && pindex->nBits == nProofOfWorkLimit)
                    pindex = pindex->pprev;
                return pindex->nBits;
            }
        }
        return pindexLast->nBits;
    }

    // Go back by what we want to be 14 days worth of blocks
    int nHeightFirst = pindexLast->nHeight - (params.DifficultyAdjustmentInterval()-1);
    assert(nHeightFirst >= 0);
    const CBlockIndex* pindexFirst = pindexLast->GetAncestor(nHeightFirst);
    assert(pindexFirst);

    return CalculateNextWorkRequired(pindexLast, pindexFirst->GetBlockTime(), params);
}

unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params& params)
{
    if (params.fPowNoRetargeting)
        return pindexLast->nBits;

    // Limit adjustment step
    int64_t nActualTimespan = pindexLast->GetBlockTime() - nFirstBlockTime;
    if (nActualTimespan < params.nPowTargetTimespan/4)
        nActualTimespan = params.nPowTargetTimespan/4;
    if (nActualTimespan > params.nPowTargetTimespan*4)
        nActualTimespan = params.nPowTargetTimespan*4;

    // Retarget
    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
    arith_uint256 bnNew;
    bnNew.SetCompact(pindexLast->nBits);
    bnNew *= nActualTimespan;
    bnNew /= params.nPowTargetTimespan;

    if (bnNew > bnPowLimit)
        bnNew = bnPowLimit;

    return bnNew.GetCompact();
}

bool CheckProofOfWork(uint256 hash, unsigned int nBits, const Consensus::Params& params)
{
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;

    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);

    // Check range
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(params.powLimit))
        return false;

    // Check proof of work matches claimed amount
    if (UintToArith256(hash) > bnTarget)
        return false;

    return true;
}
