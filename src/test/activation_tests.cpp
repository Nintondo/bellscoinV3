// Copyright (c) 2019-2023 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chain.h>
#include <chainparams.h>
#include <consensus/activation.hpp>
#include <sync.h>
#include <validation.h>

#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(activation_tests, BasicTestingSetup)

static void SetMTP(std::array<CBlockIndex, 12> &blocks, int64_t mtp) {
    size_t len = blocks.size();

    for (size_t i = 0; i < len; ++i) {
        blocks[i].nTime = mtp + (i - (len / 2));
    }

    BOOST_CHECK_EQUAL(blocks.back().GetMedianTimePast(), mtp);
}

BOOST_AUTO_TEST_CASE(isupgrade8enabled) {
    const Consensus::Params &consensus = GlobParams().GetConsensus();
    BOOST_CHECK(!IsUpgrade8Enabled(consensus, nullptr));

    std::array<CBlockIndex, 4> blocks;
    blocks[0].nHeight = consensus.upgrade8Height - 2;

    for (size_t i = 1; i < blocks.size(); ++i) {
        blocks[i].pprev = &blocks[i - 1];
        blocks[i].nHeight = blocks[i - 1].nHeight + 1;
    }
    BOOST_CHECK(!IsUpgrade8Enabled(consensus, &blocks[0]));
    BOOST_CHECK(!IsUpgrade8Enabled(consensus, &blocks[1]));
    BOOST_CHECK(IsUpgrade8Enabled(consensus, &blocks[2]));
    BOOST_CHECK(IsUpgrade8Enabled(consensus, &blocks[3]));
}

BOOST_AUTO_TEST_SUITE_END()
