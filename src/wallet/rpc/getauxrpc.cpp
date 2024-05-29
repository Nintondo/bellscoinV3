// Copyright (c) 2013-2019 The BELLSCOIN Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include <key_io.h>
#include <rpc/util.h>
#include <wallet/wallet.h>
#include <wallet/rpc/util.h>
#include <wallet/rpc/wallet.h>
#include <wallet/scriptpubkeyman.h>
#include <util/fees.h>
#include <consensus/validation.h>
#include <validation.h>
#include <rpc/auxpow_miner.h>
#include <rpc/server.h>
#include <wallet/coincontrol.h>

using namespace wallet;

namespace
{

/**
 * Helper class that keeps track of reserved keys that are used for mining
 * coinbases.  We also keep track of the block hash(es) that have been
 * constructed based on the key, so that we can mark it as keep and get a
 * fresh one when one of those blocks is submitted.
 */
class ReservedKeysForMining
{

private:

  /**
   * The per-wallet data that we store.
   */
  struct PerWallet
  {

    /**
     * The current coinbase script.  This has been taken out of the wallet
     * already (and marked as "keep"), but is reused until a block actually
     * using it is submitted successfully.
     */
    CScript coinbaseScript;

    /** All block hashes (in hex) that are based on the current script.  */
    std::set<std::string> blockHashes;

    explicit PerWallet (const CScript& scr)
      : coinbaseScript(scr)
    {}

    PerWallet (PerWallet&&) = default;

  };

  /**
   * Data for each wallet that we have.  This is keyed by CWallet::GetName,
   * which is not perfect; but it will likely work in most cases, and even
   * when two different wallets are loaded with the same name (after each
   * other), the worst that can happen is that we mine to an address from
   * the other wallet.
   */
  std::map<std::string, PerWallet> data;


public:
  /** Lock for this instance.  */
  mutable RecursiveMutex cs;
  ReservedKeysForMining () = default;

  /**
   * Retrieves the key to use for mining at the moment.
   */
  CScript
  GetCoinbaseScript (CWallet* pwallet) EXCLUSIVE_LOCKS_REQUIRED(cs)
  {
    LOCK (pwallet->cs_wallet);

    const auto mit = data.find (pwallet->GetName ());
    if (mit != data.end ())
      return mit->second.coinbaseScript;

    ReserveDestination rdest(pwallet, pwallet->m_default_address_type);
    CTxDestination dest;
    auto op_dest = rdest.GetReservedDestination(false);
    rdest.KeepDestination ();
    if (!op_dest) {
         throw JSONRPCError (RPC_WALLET_KEYPOOL_RAN_OUT,
                          "Error: Keypool ran out,"
                          " please call keypoolrefill first");
    } else {
        dest = *op_dest;
        CScript res = GetScriptForDestination (dest);
        data.emplace (pwallet->GetName (), PerWallet (res));
        return res;
    }
  }

  /**
   * Adds the block hash (given as hex string) of a newly constructed block
   * to the set of blocks for the current key.
   */
  void
  AddBlockHash (const CWallet* pwallet, const std::string& hashHex)  EXCLUSIVE_LOCKS_REQUIRED(cs)
  {
    const auto mit = data.find (pwallet->GetName ());
    assert (mit != data.end ());
    mit->second.blockHashes.insert (hashHex);
  }

  /**
   * Marks a block as submitted, releasing the key for it (if any).
   */
  void
  MarkBlockSubmitted (const CWallet* pwallet, const std::string& hashHex)  EXCLUSIVE_LOCKS_REQUIRED(cs)
  {
    const auto mit = data.find (pwallet->GetName ());
    if (mit == data.end ())
      return;

    if (mit->second.blockHashes.count (hashHex) > 0)
      data.erase (mit);
  }

};

ReservedKeysForMining g_mining_keys;

}

static RPCHelpMan getauxblock()
{
    return RPCHelpMan{"getauxblock",
                "\nCreates or submits a merge-mined block.\n"
                "\nWithout arguments, creates a new block and returns information\n"
                "required to merge-mine it.  With arguments, submits a solved\n"
                "auxpow for a previously returned block.\n",
                {
                    {"hash", RPCArg::Type::STR_HEX, RPCArg::Optional::OMITTED, "Hash of the block to submit"},
                    {"auxpow", RPCArg::Type::STR_HEX, RPCArg::Optional::OMITTED, "Serialised auxpow found"},
                },
                {
                    RPCResult{"without arguments",
                        RPCResult::Type::OBJ, "", "",
                        {
                            {RPCResult::Type::STR_HEX, "hash", "hash of the created block"},
                            {RPCResult::Type::NUM, "chainid", "chain ID for this block"},
                            {RPCResult::Type::STR_HEX, "previousblockhash", "hash of the previous block"},
                            {RPCResult::Type::NUM, "coinbasevalue", "value of the block's coinbase"},
                            {RPCResult::Type::STR, "bits", "compressed target of the block"},
                            {RPCResult::Type::NUM, "height", "height of the block"},
                            {RPCResult::Type::STR_HEX, "_target", "target in reversed byte order, deprecated"},
                        },
                    },
                    RPCResult{"with arguments",
                        RPCResult::Type::BOOL, "", "whether the submitted block was correct"
                    },
                },
                RPCExamples{
                    HelpExampleCli("getauxblock", "")
                    + HelpExampleCli("getauxblock", "\"hash\" \"serialised auxpow\"")
                    + HelpExampleRpc("getauxblock", "")
                },
    [&](const RPCHelpMan& self, const node::JSONRPCRequest& request) -> UniValue
{
    /* RPCHelpMan::Check is not applicable here since we have the
       custom check for exactly zero or two arguments.  */

    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    
    if (!wallet) return NullUniValue;

    CWallet* const pwallet = wallet.get();
    if (pwallet->IsWalletFlagSet(WALLET_FLAG_DISABLE_PRIVATE_KEYS)) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Error: Private keys are disabled for this wallet");
    }
    LOCK(g_mining_keys.cs);
    /* Create a new block */
    if (request.params.size() == 0)
    {
        const CScript coinbaseScript = g_mining_keys.GetCoinbaseScript(pwallet);
        UniValue res = AuxpowMiner::get().createAuxBlock(request, coinbaseScript);
        g_mining_keys.AddBlockHash(pwallet, res["hash"].get_str ());
        return res;
    }

    /* Submit a block instead.  */
    assert(request.params.size() == 2);
    const std::string& hash = request.params[0].get_str();

    const bool fAccepted
        = AuxpowMiner::get().submitAuxBlock(request, hash, request.params[1].get_str());
    if (fAccepted)
        g_mining_keys.MarkBlockSubmitted(pwallet, hash);

    return fAccepted;
},
    };
}

Span<const CRPCCommand> wallet::RegisterAuxRPCCommands()
{
    static const CRPCCommand commands[]{ 
        /** Auxpow wallet functions */
        {"wallet", &getauxblock},
    };
// clang-format on
    return commands;
}
