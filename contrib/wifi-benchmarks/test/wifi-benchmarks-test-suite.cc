/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ns3/eht-phy.h"
#include "ns3/he-phy.h"
#include "ns3/test.h"
#include "ns3/wifi-benchmark-helper.h"
#include "ns3/wifi-literature-reference.h"

#include <cmath>

using namespace ns3;
using namespace ns3::wifibench;

/**
 * @ingroup wifi-benchmarks
 * @ingroup tests
 *
 * @brief Check the reference formulas against the values published in the standard tables and
 *        the ns-3 PHY rate functions.
 */
class WifiBenchReferenceRatesTestCase : public TestCase
{
  public:
    WifiBenchReferenceRatesTestCase()
        : TestCase("Standard data rate formula vs published tables and ns-3 PHY")
    {
    }

  private:
    void DoRun() override
    {
        for (const auto& pub : GetPublishedRates())
        {
            const double formula =
                GetStandardDataRateMbps(pub.mcs, pub.width, pub.guardIntervalNs, pub.nss);
            NS_TEST_EXPECT_MSG_EQ_TOL(formula,
                                      pub.rateMbps,
                                      0.06 * pub.nss,
                                      "Formula differs from published rate for MCS "
                                          << +pub.mcs << " width " << pub.width);
        }
        for (uint8_t mcs = 0; mcs <= 11; ++mcs)
        {
            for (MHz_u width : {MHz_u{20}, MHz_u{40}, MHz_u{80}, MHz_u{160}})
            {
                for (uint16_t gi : {800, 1600, 3200})
                {
                    for (uint8_t nss : {1, 2, 4, 8})
                    {
                        const double ns3Rate =
                            HePhy::GetDataRate(mcs, width, NanoSeconds(gi), nss) / 1e6;
                        const double formula = GetStandardDataRateMbps(mcs, width, gi, nss);
                        NS_TEST_EXPECT_MSG_EQ_TOL(ns3Rate,
                                                  formula,
                                                  0.01,
                                                  "HePhy rate differs from the standard formula");
                    }
                }
            }
        }
        for (uint8_t mcs = 0; mcs <= 13; ++mcs)
        {
            for (MHz_u width : {MHz_u{20}, MHz_u{80}, MHz_u{320}})
            {
                const double ns3Rate = EhtPhy::GetDataRate(mcs, width, NanoSeconds(800), 1) / 1e6;
                const double formula = GetStandardDataRateMbps(mcs, width, 800, 1);
                NS_TEST_EXPECT_MSG_EQ_TOL(ns3Rate,
                                          formula,
                                          0.01,
                                          "EhtPhy rate differs from the standard formula");
            }
        }
        NS_TEST_EXPECT_MSG_EQ_TOL(GetStandardDataRateMbps(13, MHz_u{320}, 800, 8),
                                  EHT_PEAK_RATE_8SS_MBPS,
                                  0.5,
                                  "EHT peak rate");
        NS_TEST_EXPECT_MSG_EQ_TOL(GetStandardDataRateMbps(11, MHz_u{160}, 800, 8),
                                  HE_PEAK_RATE_MBPS,
                                  0.5,
                                  "HE peak rate");
        NS_TEST_EXPECT_MSG_EQ_TOL(GetStandardRuDataRateMbps(11, RuType::RU_26_TONE, 800, 1),
                                  14.7,
                                  0.06,
                                  "RU26 MCS11 rate");
        NS_TEST_EXPECT_MSG_EQ_TOL(GetStandardRequiredSnrDb(0), 4.0, 0.02, "MCS0 SNR");
        NS_TEST_EXPECT_MSG_EQ_TOL(GetStandardRequiredSnrDb(11), 34.0, 0.02, "MCS11 SNR");
        NS_TEST_EXPECT_MSG_EQ_TOL(GetMinSensitivityDbm(0, MHz_u{160}), -73.0, 1e-9, "sens160");
    }
};

/**
 * @ingroup wifi-benchmarks
 * @ingroup tests
 *
 * @brief Check the Bianchi model port against the values of the ns-3 reference script
 *        (src/wifi/examples/reference/bianchi11ax.py) tabulated in wifi-bianchi.cc.
 */
class WifiBenchBianchiTestCase : public TestCase
{
  public:
    WifiBenchBianchiTestCase()
        : TestCase("Bianchi model port vs ns-3 reference table")
    {
    }

  private:
    /**
     * Compute the throughput with the constants of the reference script.
     * @param n the number of stations
     * @param dataRateBps the data rate
     * @param ackRateBps the ACK rate
     * @return the saturation throughput in Mb/s
     */
    static double Reference(uint32_t n, double dataRateBps, double ackRateBps)
    {
        const double tSymData = 13.6;
        const double nDbps = dataRateBps * tSymData * 1e-6;
        const double bits = 16 + (30 * 8 + 1500 * 8 + 8 * 8) + 6;
        const double tData = 44 + tSymData * std::ceil(bits / nDbps);
        const double nDbpsAck = ackRateBps * 4e-6;
        const double tAck = 20 + 4 * std::ceil((16 + 14 * 8 + 6) / nDbpsAck);
        MacModelTiming timing;
        timing.aifsUs = 34;
        timing.dataPpduUs = tData;
        timing.ackPpduUs = tAck;
        timing.propagationDelayUs = 0.1;
        return ComputeBianchiThroughputMbps(n, timing, 1, 1500, true);
    }

    void DoRun() override
    {
        NS_TEST_EXPECT_MSG_EQ_TOL(Reference(5, 8.603e6, 6e6), 6.3381, 1e-3, "HeMcs0 20MHz n=5");
        NS_TEST_EXPECT_MSG_EQ_TOL(Reference(10, 8.603e6, 6e6), 5.8172, 1e-3, "HeMcs0 20MHz n=10");
        NS_TEST_EXPECT_MSG_EQ_TOL(Reference(5, 86e6, 24e6), 34.1710, 1e-3, "HeMcs7 20MHz n=5");
        NS_TEST_EXPECT_MSG_EQ_TOL(Reference(10, 86e6, 24e6), 31.9398, 1e-3, "HeMcs7 20MHz n=10");
        std::vector<double> samples{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        NS_TEST_EXPECT_MSG_EQ_TOL(Percentile(samples, 50), 5.5, 1e-9, "median");
        NS_TEST_EXPECT_MSG_EQ_TOL(Percentile(samples, 100), 10, 1e-9, "max");
    }
};

/**
 * @ingroup wifi-benchmarks
 * @ingroup tests
 *
 * @brief Test suite of the wifi-benchmarks reference models.
 */
class WifiBenchmarksTestSuite : public TestSuite
{
  public:
    WifiBenchmarksTestSuite()
        : TestSuite("wifi-benchmarks", Type::UNIT)
    {
        AddTestCase(new WifiBenchReferenceRatesTestCase, Duration::QUICK);
        AddTestCase(new WifiBenchBianchiTestCase, Duration::QUICK);
    }
};

static WifiBenchmarksTestSuite g_wifiBenchmarksTestSuite; ///< the test suite
