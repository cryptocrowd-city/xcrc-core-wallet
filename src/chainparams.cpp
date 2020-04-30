// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>

#include <chainparamsseeds.h>
#include <consensus/merkle.h>
#include <pow.h>
#include <powdata.h>
#include <tinyformat.h>
#include <util/system.h>
#include <util/strencodings.h>
#include <versionbitsinfo.h>

#include <algorithm>
#include <cassert>
#include <iostream>
#include <limits>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>

namespace
{

constexpr const char pszTimestampTestnet[] = "CRyptoCrowd Testnet";
constexpr const char pszTimestampMainnet[]
    = "29.04.2020: "
      "CNN - The world may never recover its thirst for oil";

/* Premined amount is 222,222,222 XCRC.  This is the maximum possible number of
   coins needed in case everything is sold in the ICO.  If this is not the case
   and we need to reduce the coin supply, excessive coins will be burnt by
   sending to an unspendable OP_RETURN output.  */
constexpr CAmount premineAmount = 200000000 * COIN;

/*
The premine on regtest is sent to a 1-of-2 multisig address.

The two addresses and corresponding privkeys are:
  cRH94YMZVk4MnRwPqRVebkLWerCPJDrXGN:
    b69iyynFSWcU54LqXisbbqZ8uTJ7Dawk3V3yhht6ykxgttqMQFjb
  ceREF8QnXPsJ2iVQ1M4emggoXiXEynm59D:
    b3fgAKVQpMj24gbuh6DiXVwCCjCbo1cWiZC2fXgWEU9nXy6sdxD5

This results in the multisig address: dHNvNaqcD7XPDnoRjAoyfcMpHRi5upJD7p
Redeem script:
  512103c278d06b977e67b8ea45ef24e3c96a9258c47bc4cce3d0b497b690d672497b6e21
  0221ac9dc97fe12a98374344d08b458a9c2c1df9afb29dd6089b94a3b4dc9ad57052ae

The constant below is the HASH160 of the redeem script.  In other words, the
final premine script will be:
  OP_HASH160 hexPremineAddress OP_EQUAL
*/
constexpr const char hexPremineAddressRegtest[]
    = "72240bdb175944ac843ab2c64ea6fb0371323168";

/*
The premine on testnet and mainnet is sent to a 2-of-4 multisig address.  The
keys are held by the founding members of the CRyptoCrowd team.

The address is:
  DHy2615XKevE23LVRVZVxGeqxadRGyiFW4

The hash of the redeem script is the constant below.  With it, the final
premine script is:
  OP_HASH160 hexPremineAddress OP_EQUAL
*/
constexpr const char hexPremineAddressMainnet[]
    = "72240bdb175944ac843ab2c64ea6fb0371323168";

CBlock CreateGenesisBlock(const CScript& genesisInputScript, const CScript& genesisOutputScript, uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    CMutableTransaction txNew;
    txNew.nVersion = 1;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = genesisInputScript;
    txNew.vout[0].nValue = genesisReward;
    txNew.vout[0].scriptPubKey = genesisOutputScript;

    CBlock genesis;
    genesis.nTime    = nTime;
    genesis.nBits    = 0;
    genesis.nNonce   = 0;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);

    std::unique_ptr<CPureBlockHeader> fakeHeader(new CPureBlockHeader ());
    fakeHeader->nNonce = nNonce;
    fakeHeader->hashMerkleRoot = genesis.GetHash ();
    genesis.pow.setCoreAlgo (PowAlgo::NEOSCRYPT);
    genesis.pow.setBits (nBits);
    genesis.pow.setFakeHeader (std::move (fakeHeader));

    return genesis;
}

/**
 * Build the genesis block. Note that the output of its generation
 * transaction cannot be spent since it did not originally exist in the
 * database.
 */
CBlock
CreateGenesisBlock (const uint32_t nTime, const uint32_t nNonce,
                    const uint32_t nBits,
                    const std::string& timestamp,
                    const uint160& premineP2sh)
{
  const std::vector<unsigned char> timestampData(timestamp.begin (),
                                                 timestamp.end ());
  const CScript genesisInput = CScript () << timestampData;

  std::vector<unsigned char>
    scriptHash (premineP2sh.begin (), premineP2sh.end ());
  std::reverse (scriptHash.begin (), scriptHash.end ());
  const CScript genesisOutput = CScript ()
    << OP_HASH160 << scriptHash << OP_EQUAL;

  const int32_t nVersion = 1;
  return CreateGenesisBlock (genesisInput, genesisOutput, nTime, nNonce, nBits,
                             nVersion, premineAmount);
}

/**
 * Mines the genesis block (by finding a suitable nonce only).  When done, it
 * prints the found nonce and block hash and exits.
 */
void MineGenesisBlock (CBlock& block, const Consensus::Params& consensus) 
 {
 
  std::cout << "Mining genesis block..." << std::endl;

  block.nTime = GetTime ();

  auto& fakeHeader = block.pow.initFakeHeader (block);
  while (!block.pow.checkProofOfWork (fakeHeader, consensus))
    {
      assert (fakeHeader.nNonce < std::numeric_limits<uint32_t>::max ());
      ++fakeHeader.nNonce;
      if (fakeHeader.nNonce % 1000 == 0)
        std::cout << "  nNonce = " << fakeHeader.nNonce << "..." << std::endl;
    }

  std::cout << "Found nonce: " << fakeHeader.nNonce << std::endl;
  std::cout << "nTime: " << block.nTime << std::endl;
  std::cout << "Block hash: " << block.GetHash ().GetHex () << std::endl;
  std::cout << "Merkle root: " << block.hashMerkleRoot.GetHex () << std::endl;
  exit (EXIT_SUCCESS);
  }
 
} // anonymous namespace

/**
 * Main network
 */
class CMainParams : public CChainParams {
public:
    CMainParams() {
        strNetworkID = "main";
        consensus.nSubsidyHalvingInterval = 4216320;
        /* The value of ~38 XCRC is calculated to yield the desired total
           PoW coin supply.  For the calculation, see here:

           https://github.com/cryptocrowd/cryptocrowd/issues/70#issuecomment-441292533
        */
        consensus.initialSubsidy = 10000000000;
        consensus.BIP16Height = 0;
        consensus.BIP34Height = 1;
        consensus.BIP65Height = 0;
        consensus.BIP66Height = 0;
        consensus.CSVHeight = 1;
        consensus.SegwitHeight = 0;
        consensus.MinBIP9WarningHeight = 2016; // segwit activation height + miner confirmation window
        consensus.powLimitNeoscrypt = uint256S("00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 1916; // 95% of 2016
        consensus.nMinerConfirmationWindow = 2016;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1199145601; // January 1, 2008
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1230767999; // December 31, 2008

        // The best chain should have at least this much work.
        // The value is the chain work of the CRyptoCrowd mainnet chain at height
        // 1,234,000, with best block hash:
        // a853c0581c3637726a769b77cadf185e09666742757ef2df00058e876cf25897
        consensus.nMinimumChainWork = uint256S("0000000000000000000000000000000000000000000000000000000100010001");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0xa853c0581c3637726a769b77cadf185e09666742757ef2df00058e876cf25897"); // 1,234,000

        consensus.nAuxpowChainId = 1829;

        consensus.rules.reset(new Consensus::MainNetConsensus());

        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 32-bit integer with any alignment.
         */
        pchMessageStart[0] = 0xa2;
        pchMessageStart[1] = 0xf2;
        pchMessageStart[2] = 0xf6;
        pchMessageStart[3] = 0x93;
        nDefaultPort = 11998;
        nPruneAfterHeight = 100000;
        m_assumed_blockchain_size = 2;
        m_assumed_chain_state_size = 1;

        genesis = CreateGenesisBlock (1531470713, 767681, 0x1e0ffff0,
                                      pszTimestampMainnet,
                                      uint160S (hexPremineAddressMainnet));
        consensus.hashGenesisBlock = genesis.GetHash();
                
        consensus.hashGenesisBlock = uint256S("0x");
        if (true && (genesis.GetHash() != consensus.hashGenesisBlock)) { 
        std::cout << "Mining Mainnet genesis block..." << std::endl;

        genesis.nTime = GetTime ();

        auto& fakeHeader = genesis.pow.initFakeHeader (genesis);
        while (!genesis.pow.checkProofOfWork (fakeHeader, consensus))
          {
            assert (fakeHeader.nNonce < std::numeric_limits<uint32_t>::max ());
            ++fakeHeader.nNonce;
            if (fakeHeader.nNonce % 1000 == 0)
              std::cout << "  nNonce = " << fakeHeader.nNonce << "..." << std::endl;
          }

        std::cout << "Found nonce: " << fakeHeader.nNonce << std::endl;
        std::cout << "nTime: " << genesis.nTime << std::endl;
        std::cout << "Block hash: " << genesis.GetHash ().GetHex () << std::endl;
        std::cout << "Merkle root: " << genesis.hashMerkleRoot.GetHex () << std::endl;
        }
        std::cout << std::string("Finished calculating Mainnet Genesis Block:\n");
        
//        assert(consensus.hashGenesisBlock == uint256S("0x"));
//        assert(genesis.hashMerkleRoot == uint256S("0x"));

        vSeeds.emplace_back("xcrc.seed.cryptocrowd.city");
        /* vSeeds.emplace_back("seed.cryptocrowd.domob.eu"); */

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,75);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,80);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,140);
        /* FIXME: Update these below.  */
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x88, 0xB2, 0x1E};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x88, 0xAD, 0xE4};

        bech32_hrp = "xcrc";

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_main, pnSeed6_main + ARRAYLEN(pnSeed6_main));

        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        m_is_test_chain = false;

        checkpointData = {
/*            {
                {      0, uint256S("ce46f5f898b38e9c8c5e9ae4047ef5bccc42ec8eca0142202813a625e6dc2656")},
                { 340000, uint256S("e685ccaa62025c5c5075cfee80e498589bd4788614dcbe397e12bf2b8e887e47")},
                {1234000, uint256S("a853c0581c3637726a769b77cadf185e09666742757ef2df00058e876cf25897")},  
            }  */
        };

        chainTxData = ChainTxData{
            // Data from rpc: getchaintxstats 4096 a853c0581c3637726a769b77cadf185e09666742757ef2df00058e876cf25897
            /* nTime    */ 0, // 1570402226,
            /* nTxCount */ 0, // 1924375,
            /* dTxRate  */ 0 // 0.07199120150565784,
        };
    }

    int DefaultCheckNameDB () const
    {
        return -1;
    }
};

/**
 * Testnet (v3)
 */
class CTestNetParams : public CChainParams {
public:
    CTestNetParams() {
        strNetworkID = "test";
        consensus.nSubsidyHalvingInterval = 4216320;
        consensus.initialSubsidy = 10000000000; //10 * COIN;
        consensus.BIP16Height = 0;
        consensus.BIP34Height = 1;
        consensus.BIP65Height = 0;
        consensus.BIP66Height = 0;
        consensus.CSVHeight = 1;
        consensus.SegwitHeight = 0;
        consensus.MinBIP9WarningHeight = 2016; // segwit activation height + miner confirmation window
        consensus.powLimitNeoscrypt = uint256S("00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 1512; // 75% for testchains
        consensus.nMinerConfirmationWindow = 2016;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1199145601; // January 1, 2008
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1230767999; // December 31, 2008

        // The value is the chain work of the CRyptoCrowd testnet chain at height
        // 70,000 with best block hash:
        // e2c154dc8e223cef271b54174c9d66eaf718378b30977c3df115ded629f3edb1
        consensus.nMinimumChainWork = uint256S("0000000000000000000000000000000000000000000000000000000100010001");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0xe2c154dc8e223cef271b54174c9d66eaf718378b30977c3df115ded629f3edb1"); // 70,000

        consensus.nAuxpowChainId = 1829;

        consensus.rules.reset(new Consensus::TestNetConsensus());

        pchMessageStart[0] = 0xc8;
        pchMessageStart[1] = 0xc3;
        pchMessageStart[2] = 0x95;
        pchMessageStart[3] = 0x87;
        nDefaultPort = 19998;
        nPruneAfterHeight = 1000;
        m_assumed_blockchain_size = 1;
        m_assumed_chain_state_size = 1;

        genesis = CreateGenesisBlock (1531470713, 343829, 0x1e0ffff0,
                                      pszTimestampTestnet,
                                      uint160S (hexPremineAddressMainnet));
        consensus.hashGenesisBlock = genesis.GetHash();
                
        consensus.hashGenesisBlock = uint256S("0x");
        if (true && (genesis.GetHash() != consensus.hashGenesisBlock)) { 
        std::cout << "Mining Testnet genesis block..." << std::endl;

        genesis.nTime = GetTime ();

        auto& fakeHeader = genesis.pow.initFakeHeader (genesis);
        while (!genesis.pow.checkProofOfWork (fakeHeader, consensus))
          {
            assert (fakeHeader.nNonce < std::numeric_limits<uint32_t>::max ());
            ++fakeHeader.nNonce;
            if (fakeHeader.nNonce % 1000 == 0)
              std::cout << "  nNonce = " << fakeHeader.nNonce << "..." << std::endl;
          }

        std::cout << "Found nonce: " << fakeHeader.nNonce << std::endl;
        std::cout << "nTime: " << genesis.nTime << std::endl;
        std::cout << "Block hash: " << genesis.GetHash ().GetHex () << std::endl;
        std::cout << "Merkle root: " << genesis.hashMerkleRoot.GetHex () << std::endl;
        }
        std::cout << std::string("Finished calculating Testnet Genesis Block:\n");
                
//        assert(consensus.hashGenesisBlock == uint256S("0x"));
//        assert(genesis.hashMerkleRoot == uint256S("0x"));

        vFixedSeeds.clear();
        vSeeds.clear();
        vSeeds.emplace_back("xcrc.testnet.seed.cryptocrowd.city");
        /* vSeeds.emplace_back("seed.testnet.cryptocrowd.domob.eu"); */

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,137);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,142);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,240);
        /* FIXME: Update these below.  */
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "xcrctn";

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_test, pnSeed6_test + ARRAYLEN(pnSeed6_test));

        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        m_is_test_chain = true;


        checkpointData = {
/*            {
                {     0, uint256S("3bcc29e821e7fbd374c7460306eb893725d69dbee87c4774cdcd618059b6a578")},
                { 11000, uint256S("57670b799b6645c7776e9fdbd6abff510aaed9790625dd28072d0e87a7fafcf4")},
                { 70000, uint256S("e2c154dc8e223cef271b54174c9d66eaf718378b30977c3df115ded629f3edb1")},
            }  */
        };

        chainTxData = ChainTxData{
            // Data from rpc: getchaintxstats 4096 e2c154dc8e223cef271b54174c9d66eaf718378b30977c3df115ded629f3edb1
            /* nTime    */ 0, // 1570129934,  
            /* nTxCount */ 0, // 72978, 
            /* dTxRate  */ 0 // 0.003022522434976898, 
        };
    }

    int DefaultCheckNameDB () const
    {
        return -1;
    }
};

/**
 * Regression test
 */
class CRegTestParams : public CChainParams {
public:
    explicit CRegTestParams(const ArgsManager& args) {
        strNetworkID = "regtest";
        consensus.nSubsidyHalvingInterval = 150;
        // The subsidy for regtest net is kept same as upstream Bitcoin, so
        // that we don't have to update many of the tests unnecessarily.
        consensus.initialSubsidy = 50 * COIN;
        consensus.BIP16Height = 0;
        consensus.BIP34Height = 500; // BIP34 activated on regtest (Used in functional tests)
        consensus.BIP65Height = 1351; // BIP65 activated on regtest (Used in functional tests)
        consensus.BIP66Height = 1251; // BIP66 activated on regtest (Used in functional tests)
        consensus.CSVHeight = 432; // CSV activated on regtest (Used in rpc activation tests)
        consensus.SegwitHeight = 0; // SEGWIT is always activated on regtest unless overridden
        consensus.MinBIP9WarningHeight = 0;
        consensus.powLimitNeoscrypt = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.fPowNoRetargeting = true;
        consensus.nRuleChangeActivationThreshold = 108; // 75% for testchains
        consensus.nMinerConfirmationWindow = 144; // Faster than normal for regtest (144 instead of 2016)
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x00");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x00");

        consensus.nAuxpowChainId = 1829;

        consensus.rules.reset(new Consensus::RegTestConsensus());

        pchMessageStart[0] = 0xce;
        pchMessageStart[1] = 0xb3;
        pchMessageStart[2] = 0xbb;
        pchMessageStart[3] = 0xd4;
        nDefaultPort = 18332;
        nPruneAfterHeight = 1000;
        m_assumed_blockchain_size = 0;
        m_assumed_chain_state_size = 0;

        UpdateActivationParametersFromArgs(args);

        genesis = CreateGenesisBlock (1300000000, 0, 0x207fffff,
                                      pszTimestampTestnet,
                                      uint160S (hexPremineAddressRegtest));
        consensus.hashGenesisBlock = genesis.GetHash();
        consensus.hashGenesisBlock = uint256S("0x");
        if (true && (genesis.GetHash() != consensus.hashGenesisBlock)) { 
        std::cout << "Mining Regtest genesis block..." << std::endl;

        genesis.nTime = GetTime ();

        auto& fakeHeader = genesis.pow.initFakeHeader (genesis);
        while (!genesis.pow.checkProofOfWork (fakeHeader, consensus))
          {
            assert (fakeHeader.nNonce < std::numeric_limits<uint32_t>::max ());
            ++fakeHeader.nNonce;
            if (fakeHeader.nNonce % 1000 == 0)
              std::cout << "  nNonce = " << fakeHeader.nNonce << "..." << std::endl;
          }

        std::cout << "Found nonce: " << fakeHeader.nNonce << std::endl;
        std::cout << "nTime: " << genesis.nTime << std::endl;
        std::cout << "Block hash: " << genesis.GetHash ().GetHex () << std::endl;
        std::cout << "Merkle root: " << genesis.hashMerkleRoot.GetHex () << std::endl;
        }
        std::cout << std::string("Finished calculating Regtest Genesis Block:\n");

//        assert(consensus.hashGenesisBlock == uint256S("0x"));
//        assert(genesis.hashMerkleRoot == uint256S("0x"));

        vFixedSeeds.clear(); //!< Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();      //!< Regtest mode doesn't have any DNS seeds.

        fDefaultConsistencyChecks = true;
        fRequireStandard = true;
        m_is_test_chain = true;

        checkpointData = {
/*            {
                {0, uint256S("18042820e8a9f538e77e93c500768e5be76720383cd17e9b419916d8f356c619")},
            } */
        };

        chainTxData = ChainTxData{
            0,
            0,
            0
        };

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,137);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,142);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,230);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "xcrcrt";
    }

    int DefaultCheckNameDB () const
    {
        return 0;
    }

    /**
     * Allows modifying the Version Bits regtest parameters.
     */
    void UpdateVersionBitsParameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout)
    {
        consensus.vDeployments[d].nStartTime = nStartTime;
        consensus.vDeployments[d].nTimeout = nTimeout;
    }
    void UpdateActivationParametersFromArgs(const ArgsManager& args);
};

void CRegTestParams::UpdateActivationParametersFromArgs(const ArgsManager& args)
{
    if (gArgs.IsArgSet("-bip16height")) {
        int64_t height = gArgs.GetArg("-bip16height", consensus.BIP16Height);
        if (height < -1 || height >= std::numeric_limits<int>::max()) {
            throw std::runtime_error(strprintf("Activation height %ld for BIP16 is out of valid range. Use -1 to disable BIP16.", height));
        } else if (height == -1) {
            LogPrintf("BIP16 disabled for testing\n");
            height = std::numeric_limits<int>::max();
        }
        consensus.BIP16Height = static_cast<int>(height);
    }
    if (gArgs.IsArgSet("-segwitheight")) {
        int64_t height = gArgs.GetArg("-segwitheight", consensus.SegwitHeight);
        if (height < -1 || height >= std::numeric_limits<int>::max()) {
            throw std::runtime_error(strprintf("Activation height %ld for segwit is out of valid range. Use -1 to disable segwit.", height));
        } else if (height == -1) {
            LogPrintf("Segwit disabled for testing\n");
            height = std::numeric_limits<int>::max();
        }
        consensus.SegwitHeight = static_cast<int>(height);
    }

    if (!args.IsArgSet("-vbparams")) return;

    for (const std::string& strDeployment : args.GetArgs("-vbparams")) {
        std::vector<std::string> vDeploymentParams;
        boost::split(vDeploymentParams, strDeployment, boost::is_any_of(":"));
        if (vDeploymentParams.size() != 3) {
            throw std::runtime_error("Version bits parameters malformed, expecting deployment:start:end");
        }
        int64_t nStartTime, nTimeout;
        if (!ParseInt64(vDeploymentParams[1], &nStartTime)) {
            throw std::runtime_error(strprintf("Invalid nStartTime (%s)", vDeploymentParams[1]));
        }
        if (!ParseInt64(vDeploymentParams[2], &nTimeout)) {
            throw std::runtime_error(strprintf("Invalid nTimeout (%s)", vDeploymentParams[2]));
        }
        bool found = false;
        for (int j=0; j < (int)Consensus::MAX_VERSION_BITS_DEPLOYMENTS; ++j) {
            if (vDeploymentParams[0] == VersionBitsDeploymentInfo[j].name) {
                UpdateVersionBitsParameters(Consensus::DeploymentPos(j), nStartTime, nTimeout);
                found = true;
                LogPrintf("Setting version bits activation parameters for %s to start=%ld, timeout=%ld\n", vDeploymentParams[0], nStartTime, nTimeout);
                break;
            }
        }
        if (!found) {
            throw std::runtime_error(strprintf("Invalid deployment (%s)", vDeploymentParams[0]));
        }
    }
}

static std::unique_ptr<const CChainParams> globalChainParams;

const CChainParams &Params() {
    assert(globalChainParams);
    return *globalChainParams;
}

std::unique_ptr<const CChainParams> CreateChainParams(const std::string& chain)
{
    if (chain == CBaseChainParams::MAIN)
        return std::unique_ptr<CChainParams>(new CMainParams());
    else if (chain == CBaseChainParams::TESTNET)
        return std::unique_ptr<CChainParams>(new CTestNetParams());
    else if (chain == CBaseChainParams::REGTEST)
        return std::unique_ptr<CChainParams>(new CRegTestParams(gArgs));
    throw std::runtime_error(strprintf("%s: Unknown chain %s.", __func__, chain));
}

void SelectParams(const std::string& network)
{
    SelectBaseParams(network);
    globalChainParams = CreateChainParams(network);
}

int64_t
AvgTargetSpacing (const Consensus::Params& params, const unsigned height)
{
  /* The average target spacing for any block (all algorithms combined) is
     computed by dividing some common multiple timespan of all spacings
     by the number of blocks expected (all algorithms together) in that
     time span.

     The numerator is simply the product of all block times, while the
     denominator is a sum of products that just excludes the current
     algorithm (i.e. of all (N-1) tuples selected from the N algorithm
     block times).  */
  int64_t numer = 1;
  int64_t denom = 0;
  for (const PowAlgo algo : {PowAlgo::SHA256D, PowAlgo::NEOSCRYPT})
    {
      const int64_t spacing = params.rules->GetTargetSpacing(algo, height);

      /* Multiply all previous added block counts by this target spacing.  */
      denom *= spacing;

      /* Add the number of blocks for the current algorithm to the denominator.
         This starts off with the product of all already-processed algorithms
         (excluding the current one), and will be multiplied later on by
         the still-to-be-processed ones (in the line above).  */
      denom += numer;

      /* The numerator is the product of all spacings.  */
      numer *= spacing;
    }

  assert (denom > 0);
  assert (numer % denom == 0);
  return numer / denom;
}
