// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2013 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BELLSCOIN_PRIMITIVES_PUREHEADER_H
#define BELLSCOIN_PRIMITIVES_PUREHEADER_H

#include <serialize.h>
#include <uint256.h>
/**
 * A block header without auxpow information.  This "intermediate step"
 * in constructing the full header is useful, because it breaks the cyclic
 * dependency between auxpow (referencing a parent block header) and
 * the block header (referencing an auxpow).  The parent block header
 * does not have auxpow itself, so it is a pure header.
 */
class CPureBlockHeader
{
public:
    static const uint32_t CHAINID = 16; // should match params.nAuxpowChainId

private:
    /* Modifiers to the version.  */
    static const int32_t VERSION_AUXPOW = (1 << 8);
    static const uint8_t VERSION_START_BIT = 16;
    /** Bits above are reserved for the auxpow chain ID.  */
    static const int32_t VERSION_CHAIN_START = (1 << 16);
    static const int32_t VERSIONAUXPOW_TOP_MASK = (1 << 28) + (1 << 29) + (1 << 30);
    // mask to get Chain ID from version field, chainid is 16 in BELLS so 0x001f mask should be OK
    static const int32_t MASK_AUXPOW_CHAINID_SHIFTED = (0x001f << VERSION_START_BIT);
    // shifted Chain ID in version field
    static const int32_t VERSION_AUXPOW_CHAINID_SHIFTED = (CHAINID << VERSION_START_BIT);

public:
    // header
    int32_t nVersion;
    uint256 hashPrevBlock;
    uint256 hashMerkleRoot;
    uint32_t nTime;
    uint32_t nBits;
    uint32_t nNonce;

    CPureBlockHeader()
    {
        SetNull();
    }

    SERIALIZE_METHODS(CPureBlockHeader, obj) { READWRITE(obj.nVersion, obj.hashPrevBlock, obj.hashMerkleRoot, obj.nTime, obj.nBits, obj.nNonce); }

    void SetNull()
    {
        nVersion = 0;
        hashPrevBlock.SetNull();
        hashMerkleRoot.SetNull();
        nTime = 0;
        nBits = 0;
        nNonce = 0;
    }

    bool IsNull() const
    { 
        return (nBits == 0);
    }

    uint256 GetHash() const;

    int64_t GetBlockTime() const
    {
        return (int64_t)nTime;
    }

    uint256 GetPoWHash() const;
    /**
     * Extract the base version (without modifiers and chain ID).
     * @return The base version./
     */
    inline int32_t GetBaseVersion() const
    {
        return GetBaseVersion(nVersion);
    }

    static int32_t GetBaseVersion(const int32_t &ver);

    /**
     * Set the base version (apart from chain ID and auxpow flag) to
     * the one given.  This should only be called when auxpow is not yet
     * set, to initialise a block!
     * @param nBaseVersion The base version.
     * @param nChainId The auxpow chain ID.
     */
    void SetBaseVersion(int32_t nBaseVersion, const int32_t &nChainId);
    /**
     * Extract the chain ID.
     * @return The chain ID encoded in the version.
     */
    inline int32_t GetChainId() const
    {
        return GetChainId(nVersion);
    }

    static int32_t GetChainId(const int32_t &ver);

    static inline bool IsValidBaseVersion(const int32_t &nBaseVersion) {
        return (nBaseVersion & ~VERSIONAUXPOW_TOP_MASK) < VERSION_CHAIN_START;
    }
    /**
     * Set the chain ID.  This is used for the test suite.
     * @param chainId The chain ID to set.
     */
    inline void SetChainId(const int32_t &chainId)
    {
        nVersion %= VERSION_CHAIN_START;
        nVersion |= chainId * VERSION_CHAIN_START;
    }

    /**
     * Check if the auxpow flag is set in the version.
     * @return True iff this block version is marked as auxpow.
     */
    inline bool IsAuxpow() const
    {
        return nVersion & VERSION_AUXPOW;
    }

    /**
     * Set the auxpow flag.  This is used for testing.
     * @param auxpow Whether to mark auxpow as true.
     */
    inline void SetAuxpowVersion (bool auxpow)
    {
        if (auxpow)
            nVersion |= VERSION_AUXPOW;
        else
            nVersion &= ~VERSION_AUXPOW;
    }
    
    /**
     * Check whether this is a "legacy" block without chain ID.
     * @return True iff it is.
     */
    inline bool IsLegacy() const
    {
        return nVersion == 1;
    }
};

#endif // PRIMITIVES_PUREHEADER_H
