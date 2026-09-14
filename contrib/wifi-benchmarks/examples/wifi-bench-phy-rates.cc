/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file
 * @ingroup wifi-benchmarks
 *
 * PHY data rates and single-user MAC efficiency of 802.11ax (Wi-Fi 6) and 802.11be (Wi-Fi 7).
 *
 * Part A compares, without simulation, the HE/EHT data rates computed by ns-3
 * (HePhy::GetDataRate, EhtPhy::GetDataRate) with an independent implementation of the formula of
 * the standard MCS tables (N_SD * N_BPSCS * R * N_SS / T_SYM) and with values published in
 * IEEE Std 802.11ax-2021 (Clause 27.5), IEEE Std 802.11be-2024 (Clause 36.5) and the survey
 * literature: peak rates of 9.6 Gb/s (Wi-Fi 6, 8 SS) and 23 Gb/s (Wi-Fi 7, 8 SS, 320 MHz), the
 * 20% gain of 4096-QAM over 1024-QAM, and the doubling from 160 MHz to 320 MHz.
 *
 * Part B simulates a saturated UDP downlink between an AP and one STA at fixed MCS and compares
 * the goodput with an analytical single-user bound (AIFS + mean backoff + A-MPDU + SIFS +
 * BlockAck) that uses the PPDU durations of the standard.
 *
 * Example: ./ns3 run "wifi-bench-phy-rates --standard=be --quick"
 */

#include "ns3/command-line.h"
#include "ns3/eht-phy.h"
#include "ns3/he-phy.h"
#include "ns3/simulator.h"
#include "ns3/wifi-benchmark-helper.h"
#include "ns3/wifi-literature-reference.h"

#include <cmath>

using namespace ns3;
using namespace ns3::wifibench;

/**
 * Run one saturated single-user simulation.
 *
 * @param standard the standard
 * @param mcs the MCS
 * @param width the channel width
 * @param nss the number of spatial streams
 * @param gi the guard interval in ns
 * @param payload the UDP payload size (including the 20-byte header)
 * @param simTime the measurement duration
 * @param downlink whether traffic is DL (AP to STA) or UL
 * @param rtsCts whether RTS/CTS is used
 * @param verbose whether to print the transmission accounting
 * @return the goodput in Mb/s
 */
double
RunSingleUser(WifiStandard standard,
              uint8_t mcs,
              MHz_u width,
              uint8_t nss,
              uint16_t gi,
              uint32_t payload,
              Time simTime,
              bool downlink,
              bool rtsCts,
              bool verbose)
{
    const Band band = (width > MHz_u{160}) ? Band::GHZ_6 : Band::GHZ_5;
    NodeContainer apNode;
    apNode.Create(1);
    NodeContainer staNodes;
    staNodes.Create(1);

    BssBuilder builder;
    LinkConfig link;
    link.band = band;
    link.width = width;
    PhyConfig phy;
    phy.nss = nss;
    RateConfig rate;
    rate.mcs = mcs;
    MacConfig mac;
    mac.guardIntervalNs = gi;
    mac.rtsCts = rtsCts;
    builder.SetStandard(standard).AddLink(link).SetPhy(phy).SetRate(rate).SetMac(mac).SetChannel(
        band,
        CreateSpectrumChannel(band));
    Bss bss = builder.Build(apNode, staNodes);
    PlaceOnCircle(bss, 1.0);
    InstallInternet(bss, 1);
    PopulateArpCaches();

    const double phyRateMbps = GetStandardDataRateMbps(mcs, width, gi, nss);
    TxAccounting txAccounting;
    txAccounting.Enable(bss.AllDevices());
    FlowSet flows;
    const Time start = MilliSeconds(100);
    flows.SetMeasurementWindow(start + MilliSeconds(100), start + MilliSeconds(100) + simTime);
    txAccounting.SetWindow(start + MilliSeconds(100), start + MilliSeconds(100) + simTime);
    if (downlink)
    {
        flows.AddUdpFlow("dl",
                         apNode.Get(0),
                         staNodes.Get(0),
                         bss.staIfs.GetAddress(0),
                         phyRateMbps * 1.2,
                         payload,
                         start,
                         start + MilliSeconds(200) + simTime);
    }
    else
    {
        flows.AddUdpFlow("ul",
                         staNodes.Get(0),
                         apNode.Get(0),
                         bss.apIf.GetAddress(0),
                         phyRateMbps * 1.2,
                         payload,
                         start,
                         start + MilliSeconds(200) + simTime);
    }
    Simulator::Stop(start + MilliSeconds(200) + simTime);
    Simulator::Run();
    const double goodput = flows.GetStats(0).throughputMbps;
    if (verbose)
    {
        std::cout << "  TX accounting: " << txAccounting.Summary() << std::endl;
        std::map<uint32_t, uint32_t> hist;
        for (const auto& r : txAccounting.GetRecords())
        {
            if (r.nMpdus > 1)
            {
                hist[r.nMpdus]++;
            }
        }
        for (const auto& [n, count] : hist)
        {
            std::cout << "    A-MPDUs of " << n << " MPDUs: " << count << std::endl;
        }
    }
    Simulator::Destroy();
    return goodput;
}

int
main(int argc, char* argv[])
{
    CommonArgs common;
    std::string standardStr{"ax"};
    bool simulate{true};
    Time simTime{Seconds(1)};
    uint32_t payload{1472};
    bool downlink{true};
    bool rtsCts{false};
    double tolerance{0.06};

    CommandLine cmd(__FILE__);
    AddCommonArgs(cmd, common);
    cmd.AddValue("standard", "ax (Wi-Fi 6) or be (Wi-Fi 7)", standardStr);
    cmd.AddValue("simulate", "Run the single-user goodput simulations (part B)", simulate);
    cmd.AddValue("simulationTime", "Measurement duration of each simulation", simTime);
    cmd.AddValue("payloadSize", "UDP payload size in bytes", payload);
    cmd.AddValue("downlink", "Downlink (AP to STA) if true, uplink otherwise", downlink);
    cmd.AddValue("useRts", "Enable RTS/CTS", rtsCts);
    cmd.AddValue("tolerance", "Relative tolerance of the goodput checks", tolerance);
    cmd.Parse(argc, argv);
    ApplyCommonArgs(common);

    const WifiStandard standard = ParseStandard(standardStr);
    const bool eht = (standard == WIFI_STANDARD_80211be);
    const uint8_t maxMcs = MaxMcs(standard);
    const std::string name = eht ? "wifi7-phy-rates" : "wifi6-phy-rates";

    PrintBanner("PHY data rates and single-user MAC efficiency: " + StandardName(standard),
                "Part A: ns-3 PHY rates vs the standard formula and published values. Part B: "
                "saturated single-user goodput vs the analytical bound.",
                {"ieee80211ax", "ieee80211be", "khorov2019", "lopezperez2019", "deng2020"});

    ResultTable table(name, common.outputDir);

    // Part A: formula vs ns-3 for all combinations
    std::vector<MHz_u> widths{MHz_u{20}, MHz_u{40}, MHz_u{80}, MHz_u{160}};
    if (eht)
    {
        widths.push_back(MHz_u{320});
    }
    uint32_t combinations = 0;
    uint32_t mismatches = 0;
    double maxAbsDiff = 0;
    for (uint8_t mcs = 0; mcs <= maxMcs; ++mcs)
    {
        for (auto width : widths)
        {
            for (uint16_t gi : {800, 1600, 3200})
            {
                for (uint8_t nss = 1; nss <= 8; ++nss)
                {
                    const double ns3Rate =
                        (eht ? EhtPhy::GetDataRate(mcs, width, NanoSeconds(gi), nss)
                             : HePhy::GetDataRate(mcs, width, NanoSeconds(gi), nss)) /
                        1e6;
                    const double formula = GetStandardDataRateMbps(mcs, width, gi, nss);
                    ++combinations;
                    const double diff = std::abs(ns3Rate - formula);
                    maxAbsDiff = std::max(maxAbsDiff, diff);
                    if (diff > 0.05)
                    {
                        ++mismatches;
                    }
                }
            }
        }
    }
    {
        ResultRow row;
        row.Set("test", "formula_vs_ns3_all_mcs_width_gi_nss");
        row.Set("combinations", combinations);
        row.Set("mismatches", mismatches);
        row.Set("max_abs_diff_mbps", maxAbsDiff, 4);
        row.Set("measured", static_cast<double>(mismatches), 0);
        Compare(row, mismatches, 0, 0, "ieee80211ax", CheckKind::AT_MOST);
        table.AddRow(row);
    }

    // Part A: published spot values
    for (const auto& pub : GetPublishedRates())
    {
        if (pub.standard != standard)
        {
            continue;
        }
        const double ns3Rate = (eht ? EhtPhy::GetDataRate(pub.mcs,
                                                          pub.width,
                                                          NanoSeconds(pub.guardIntervalNs),
                                                          pub.nss)
                                    : HePhy::GetDataRate(pub.mcs,
                                                         pub.width,
                                                         NanoSeconds(pub.guardIntervalNs),
                                                         pub.nss)) /
                               1e6;
        ResultRow row;
        row.Set("test", "published_rate");
        row.Set("mcs", pub.mcs);
        row.Set("width_mhz", static_cast<double>(pub.width), 0);
        row.Set("gi_ns", pub.guardIntervalNs);
        row.Set("nss", pub.nss);
        row.Set("measured", ns3Rate, 2);
        Compare(row, ns3Rate, pub.rateMbps, 0.001, pub.source, CheckKind::WITHIN);
        table.AddRow(row);
    }

    // Part A: headline figures from the literature
    if (eht)
    {
        ResultRow gain;
        gain.Set("test", "4096qam_gain_mcs13_vs_mcs11");
        const double g =
            EhtPhy::GetDataRate(13, MHz_u{320}, NanoSeconds(800), 1) /
            static_cast<double>(EhtPhy::GetDataRate(11, MHz_u{320}, NanoSeconds(800), 1));
        gain.Set("measured", g, 4);
        Compare(gain, g, EHT_4096QAM_GAIN, 0.001, "lopezperez2019");
        table.AddRow(gain);

        ResultRow dbl;
        dbl.Set("test", "320mhz_vs_160mhz_ratio");
        const double r =
            EhtPhy::GetDataRate(13, MHz_u{320}, NanoSeconds(800), 1) /
            static_cast<double>(EhtPhy::GetDataRate(13, MHz_u{160}, NanoSeconds(800), 1));
        dbl.Set("measured", r, 4);
        Compare(dbl, r, 2.0, 0.001, "deng2020");
        table.AddRow(dbl);

        ResultRow peak16;
        peak16.Set("test", "peak_rate_16ss_extrapolated");
        const double p16 = GetStandardDataRateMbps(13, MHz_u{320}, 800, 1) * 16;
        peak16.Set("measured", p16, 1);
        Compare(peak16, p16, EHT_PEAK_RATE_16SS_MBPS, 0.001, "deng2020");
        table.AddRow(peak16);
    }
    else
    {
        ResultRow peak;
        peak.Set("test", "peak_rate_8ss");
        const double p = HePhy::GetDataRate(11, MHz_u{160}, NanoSeconds(800), 8) / 1e6;
        peak.Set("measured", p, 1);
        Compare(peak, p, HE_PEAK_RATE_MBPS, 0.001, "khorov2019");
        table.AddRow(peak);
    }

    // Part B: simulations
    if (simulate)
    {
        struct Config
        {
            uint8_t mcs;
            MHz_u width;
            uint8_t nss;
        };

        std::vector<Config> configs;
        if (common.quick)
        {
            configs = {{maxMcs, MHz_u{80}, 1}};
            simTime = std::min(simTime, MilliSeconds(300));
        }
        else
        {
            configs = {{0, MHz_u{20}, 1},
                       {7, MHz_u{80}, 1},
                       {maxMcs, MHz_u{20}, 1},
                       {maxMcs, MHz_u{80}, 1},
                       {maxMcs, MHz_u{80}, 2},
                       {maxMcs, MHz_u{160}, 1},
                       {maxMcs, MHz_u{160}, 2}};
            if (eht)
            {
                configs.push_back({11, MHz_u{320}, 1});
                configs.push_back({13, MHz_u{320}, 1});
                configs.push_back({13, MHz_u{320}, 2});
            }
        }
        const uint16_t gi = 800;
        for (const auto& cfg : configs)
        {
            const Band band = (cfg.width > MHz_u{160}) ? Band::GHZ_6 : Band::GHZ_5;
            const uint16_t baWindow = eht ? EHT_MAX_BA_WINDOW : HE_MAX_BA_WINDOW;
            const uint32_t maxAmpdu = eht ? EHT_MAX_AMPDU_BYTES : HE_MAX_AMPDU_BYTES;
            const uint32_t mpduBytes = GetMpduBytes(payload);
            const auto [k, ppduUs] = SaturatedAmpdu(mpduBytes,
                                                    baWindow,
                                                    maxAmpdu,
                                                    MAX_PPDU_DURATION_US,
                                                    standard,
                                                    cfg.mcs,
                                                    cfg.width,
                                                    gi,
                                                    cfg.nss,
                                                    band);
            const std::string ctrlMode = ControlModeName(standard, band, cfg.mcs);
            MacModelTiming timing;
            timing.aifsUs = SIFS_US + GetStandardEdcaParams(AC_BE).aifsn * SLOT_US;
            timing.dataPpduUs = ppduUs;
            timing.ackPpduUs = ControlPpduDurationUs(GetBlockAckBytes(baWindow), ctrlMode, band);
            if (rtsCts)
            {
                // RTS (20 bytes) + SIFS + CTS (14 bytes) + SIFS
                timing.dataPpduUs += ControlPpduDurationUs(20, ctrlMode, band) + SIFS_US +
                                     ControlPpduDurationUs(14, ctrlMode, band) + SIFS_US;
            }
            const double bound = ComputeSingleUserGoodputMbps(timing, k, payload);
            const double phyRate = GetStandardDataRateMbps(cfg.mcs, cfg.width, gi, cfg.nss);

            std::cout << "Simulating " << StandardName(standard) << " MCS " << +cfg.mcs << " "
                      << cfg.width << " MHz " << +cfg.nss << " SS: PHY rate " << phyRate
                      << " Mb/s, A-MPDU of " << k << " MPDUs (" << ppduUs
                      << " us), analytical goodput bound " << bound << " Mb/s" << std::endl;
            const double goodput = RunSingleUser(standard,
                                                 cfg.mcs,
                                                 cfg.width,
                                                 cfg.nss,
                                                 gi,
                                                 payload,
                                                 simTime,
                                                 downlink,
                                                 rtsCts,
                                                 common.verbose);
            ResultRow row;
            row.Set("test", "single_user_goodput");
            row.Set("mcs", cfg.mcs);
            row.Set("width_mhz", static_cast<double>(cfg.width), 0);
            row.Set("gi_ns", gi);
            row.Set("nss", cfg.nss);
            row.Set("band", BandName(band));
            row.Set("phy_rate_mbps", phyRate, 1);
            row.Set("ampdu_mpdus", k);
            row.Set("ppdu_us", ppduUs, 1);
            row.Set("ba_us", timing.ackPpduUs, 1);
            row.Set("mac_efficiency_pct", goodput / phyRate * 100, 1);
            row.Set("measured", goodput, 2);
            Compare(row, goodput, bound, tolerance, "ieee80211-2020", CheckKind::WITHIN);
            table.AddRow(row);
        }
    }

    table.Print();
    table.Write();
    const auto fails = table.PrintSummary();
    return fails ? 1 : 0;
}
