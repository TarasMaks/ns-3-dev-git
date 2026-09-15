/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file
 * @ingroup wifi-benchmarks
 *
 * Downlink OFDMA (802.11ax/802.11be) vs single-user transmissions.
 *
 * An AP serves N stations. Two traffic scenarios are simulated with and without DL OFDMA (round
 * robin scheduler, DL MU PPDUs acknowledged with an aggregated MU-BAR trigger):
 * - saturated downlink: aggregate goodput vs the PHY bound (sum of the per-RU data rates of the
 *   standard, Clause 27.5 / 36.5 RU tables) and MAC efficiency;
 * - low load with small packets (Poisson arrivals): latency percentiles and airtime, which is the
 *   regime where the literature reports OFDMA gains (Khorov et al. 2019, Deng et al. 2020).
 *
 * Example: ./ns3 run "wifi-bench-dl-ofdma --channelWidth=20 --nStations=4,8 --mcs=7"
 */

#include "ns3/command-line.h"
#include "ns3/simulator.h"
#include "ns3/wifi-benchmark-helper.h"
#include "ns3/wifi-literature-reference.h"

#include <map>
#include <sstream>

using namespace ns3;
using namespace ns3::wifibench;

/**
 * @param s a comma separated list of integers
 * @return the integers
 */
std::vector<uint32_t>
ParseList(const std::string& s)
{
    std::vector<uint32_t> out;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ','))
    {
        if (!item.empty())
        {
            out.push_back(std::stoul(item));
        }
    }
    return out;
}

/// Outcome of one run
struct RunResult
{
    FlowStats aggregate;                ///< aggregate flow statistics
    double minStaMbps{0};               ///< minimum per-STA goodput
    double maxStaMbps{0};               ///< maximum per-STA goodput
    RuType ruType{RuType::RU_TYPE_MAX}; ///< dominant RU type of the DL MU PPDUs
    double meanUsersPerMuPpdu{0};       ///< mean number of users per DL MU PPDU
    uint64_t muPpdus{0};                ///< number of DL MU PPDUs
    uint64_t suPpdus{0};                ///< number of SU data PPDUs
    double airtimeFraction{0};          ///< fraction of time with PPDUs on the air
};

/**
 * Run one scenario.
 * @param standard the standard
 * @param n the number of STAs
 * @param mcs the MCS
 * @param width the channel width
 * @param ofdma whether DL OFDMA is enabled
 * @param saturated saturated traffic (else low-load small packets)
 * @param payload the payload size
 * @param loadMbpsPerSta the per-STA offered load (low-load scenario)
 * @param simTime the measurement duration
 * @param ackType the DL MU ack sequence type
 * @param verbose print details
 * @return the run result
 */
RunResult
Run(WifiStandard standard,
    uint32_t n,
    uint8_t mcs,
    MHz_u width,
    bool ofdma,
    bool saturated,
    uint32_t payload,
    double loadMbpsPerSta,
    Time simTime,
    const std::string& ackType,
    bool verbose)
{
    const bool eht = (standard == WIFI_STANDARD_80211be);
    const Band band = (width > MHz_u{160}) ? Band::GHZ_6 : Band::GHZ_5;
    NodeContainer apNode;
    apNode.Create(1);
    NodeContainer staNodes;
    staNodes.Create(n);
    BssBuilder builder;
    LinkConfig link;
    link.band = band;
    link.width = width;
    RateConfig rate;
    rate.mcs = mcs;
    MacConfig mac;
    if (ofdma)
    {
        MuSchedulerConfig mu;
        mu.dlAckType = ackType;
        mu.nStations = n;
        builder.SetMuScheduler(mu);
    }
    builder.SetStandard(standard).AddLink(link).SetRate(rate).SetMac(mac).SetChannel(
        band,
        CreateSpectrumChannel(band));
    Bss bss = builder.Build(apNode, staNodes);
    PlaceOnCircle(bss, 2.0);
    InstallInternet(bss, 1);
    PopulateArpCaches();

    TxAccounting txAccounting;
    txAccounting.Enable(bss.AllDevices());
    FlowSet flows;
    const Time start = MilliSeconds(100);
    const Time mStart = start + MilliSeconds(300);
    flows.SetMeasurementWindow(mStart, mStart + simTime);
    txAccounting.SetWindow(mStart, mStart + simTime);
    const double phyRate = GetStandardDataRateMbps(mcs, width, 800, 1);
    for (uint32_t i = 0; i < n; ++i)
    {
        flows.AddUdpFlow("dl" + std::to_string(i),
                         apNode.Get(0),
                         staNodes.Get(i),
                         bss.staIfs.GetAddress(i),
                         saturated ? phyRate * 1.2 / n : loadMbpsPerSta,
                         payload,
                         start,
                         mStart + simTime + MilliSeconds(1),
                         0,
                         saturated ? TrafficPattern::CBR : TrafficPattern::POISSON);
    }
    Simulator::Stop(mStart + simTime + MilliSeconds(1));
    Simulator::Run();

    RunResult res;
    res.aggregate = flows.GetAggregateStats();
    res.minStaMbps = flows.GetThroughputPercentile(0);
    res.maxStaMbps = flows.GetThroughputPercentile(100);
    const std::string muPreamble = eht ? "EHT_MU" : "HE_MU";
    res.ruType = txAccounting.GetDominantRuType(muPreamble);
    // with EHT, SU PPDUs also use the EHT_MU preamble: distinguish by the number of users
    double users = 0;
    for (const auto& r : txAccounting.GetRecords())
    {
        if (r.preamble == muPreamble && r.nUsers > 1)
        {
            res.muPpdus++;
            users += r.nUsers;
        }
        else if ((r.preamble == muPreamble || r.preamble == "HE_SU") && r.nMpdus > 0 &&
                 r.nUsers == 1 && r.psduBytes > 100)
        {
            res.suPpdus++;
        }
    }
    // PPDUs starting at the same time (TB PPDUs of different STAs) occupy the medium once
    std::map<Time, double> airtimeByStart;
    for (const auto& r : txAccounting.GetRecords())
    {
        airtimeByStart[r.time] = std::max(airtimeByStart[r.time], r.durationUs);
    }
    double airtimeUs = 0;
    for (const auto& [t, d] : airtimeByStart)
    {
        airtimeUs += d;
    }
    res.airtimeFraction = airtimeUs / simTime.GetMicroSeconds();
    res.meanUsersPerMuPpdu = res.muPpdus ? users / res.muPpdus : 0;
    if (verbose)
    {
        std::cout << "  n=" << n << (ofdma ? " OFDMA" : " SU")
                  << (saturated ? " saturated" : " low-load") << ": " << txAccounting.Summary()
                  << std::endl;
    }
    Simulator::Destroy();
    return res;
}

int
main(int argc, char* argv[])
{
    CommonArgs common;
    std::string standardStr{"ax"};
    uint32_t mcs{7};
    double channelWidthMhz{20};
    std::string nStationsStr{"2,4,8"};
    uint32_t payload{1472};
    uint32_t smallPayload{200};
    double lowLoadMbpsPerSta{1.0};
    Time simTime{Seconds(2)};
    std::string ackType{"AGGR-MU-BAR"};

    CommandLine cmd(__FILE__);
    AddCommonArgs(cmd, common);
    cmd.AddValue("standard", "ax (Wi-Fi 6) or be (Wi-Fi 7)", standardStr);
    cmd.AddValue("mcs", "MCS index", mcs);
    cmd.AddValue("channelWidth", "Channel width in MHz", channelWidthMhz);
    cmd.AddValue("nStations", "Comma separated list of numbers of stations", nStationsStr);
    cmd.AddValue("payloadSize", "UDP payload size (saturated scenario)", payload);
    cmd.AddValue("smallPayloadSize", "UDP payload size (low-load scenario)", smallPayload);
    cmd.AddValue("lowLoadMbpsPerSta",
                 "Per-STA offered load in the low-load scenario",
                 lowLoadMbpsPerSta);
    cmd.AddValue("simulationTime", "Measurement duration of each simulation", simTime);
    cmd.AddValue("dlAckType", "DL MU ack sequence: ACK-SU-FORMAT, MU-BAR or AGGR-MU-BAR", ackType);
    cmd.Parse(argc, argv);
    ApplyCommonArgs(common);

    const WifiStandard standard = ParseStandard(standardStr);
    const bool eht = (standard == WIFI_STANDARD_80211be);
    const MHz_u width{channelWidthMhz};
    const std::string name = eht ? "wifi7-dl-ofdma" : "wifi6-dl-ofdma";
    auto nList = ParseList(nStationsStr);
    if (common.quick)
    {
        nList = {4};
        simTime = std::min(simTime, MilliSeconds(300));
    }

    PrintBanner("Downlink OFDMA vs single-user: " + StandardName(standard),
                "Saturated goodput vs the per-RU PHY bound of the standard, and latency/airtime "
                "with small packets at low load.",
                {"ieee80211ax", "ieee80211be", "khorov2019", "deng2020"});

    ResultTable table(name, common.outputDir);
    const double phyRate = GetStandardDataRateMbps(mcs, width, 800, 1);
    for (const auto n : nList)
    {
        for (const bool saturated : {true, false})
        {
            RunResult su = Run(standard,
                               n,
                               mcs,
                               width,
                               false,
                               saturated,
                               saturated ? payload : smallPayload,
                               lowLoadMbpsPerSta,
                               simTime,
                               ackType,
                               common.verbose);
            RunResult mu = Run(standard,
                               n,
                               mcs,
                               width,
                               true,
                               saturated,
                               saturated ? payload : smallPayload,
                               lowLoadMbpsPerSta,
                               simTime,
                               ackType,
                               common.verbose);
            std::ostringstream ruStr;
            double ruRate = 0;
            if (mu.ruType != RuType::RU_TYPE_MAX)
            {
                ruStr << mu.ruType;
                ruRate = GetStandardRuDataRateMbps(mcs, mu.ruType, 800, 1);
            }
            else
            {
                ruStr << "none";
            }
            // the scheduler may serve fewer stations than n in a MU PPDU (e.g. 4 RUs of 52
            // tones in 20 MHz): the PHY bound is the RU rate times the users actually served
            const double ofdmaPhyBound = ruRate * mu.meanUsersPerMuPpdu;

            for (const bool isOfdma : {false, true})
            {
                const RunResult& r = isOfdma ? mu : su;
                ResultRow row;
                row.Set("scenario", saturated ? "saturated" : "low-load-small-packets");
                row.Set("n_stations", n);
                row.Set("mode", isOfdma ? "DL-OFDMA" : "SU");
                row.Set("mcs", mcs);
                row.Set("width_mhz", channelWidthMhz, 0);
                row.Set("payload_bytes", saturated ? payload : smallPayload);
                row.Set("ru_type", isOfdma ? ruStr.str() : "full");
                row.Set("users_per_mu_ppdu", isOfdma ? mu.meanUsersPerMuPpdu : 1.0, 2);
                row.Set("phy_bound_mbps", isOfdma ? ofdmaPhyBound : phyRate, 1);
                row.Set("mu_ppdus", r.muPpdus);
                row.Set("su_ppdus", r.suPpdus);
                row.Set("min_sta_mbps", r.minStaMbps, 2);
                row.Set("max_sta_mbps", r.maxStaMbps, 2);
                row.Set("airtime_fraction", r.airtimeFraction, 3);
                row.Set("latency_p50_ms", r.aggregate.p50LatencyMs, 3);
                row.Set("latency_p95_ms", r.aggregate.p95LatencyMs, 3);
                row.Set("latency_p99_ms", r.aggregate.p99LatencyMs, 3);
                row.Set("mac_efficiency_pct",
                        r.aggregate.throughputMbps / (isOfdma ? ofdmaPhyBound : phyRate) * 100,
                        1);
                if (saturated)
                {
                    row.Set("measured", r.aggregate.throughputMbps, 2);
                    Compare(row,
                            r.aggregate.throughputMbps,
                            isOfdma ? ofdmaPhyBound : phyRate,
                            0.0,
                            "ieee80211ax",
                            CheckKind::AT_MOST);
                }
                else
                {
                    row.Set("measured", r.aggregate.p95LatencyMs, 3);
                    row.Set("loss_ratio", r.aggregate.lossRatio, 4);
                    Compare(row,
                            r.aggregate.p95LatencyMs,
                            su.aggregate.p95LatencyMs,
                            0.0,
                            "khorov2019",
                            CheckKind::INFO);
                }
                table.AddRow(row);
            }
            ResultRow gain;
            gain.Set("scenario", saturated ? "saturated" : "low-load-small-packets");
            gain.Set("n_stations", n);
            gain.Set("mode", "gain-OFDMA-vs-SU");
            gain.Set("mcs", mcs);
            gain.Set("width_mhz", channelWidthMhz, 0);
            if (saturated)
            {
                const double g = su.aggregate.throughputMbps > 0
                                     ? mu.aggregate.throughputMbps / su.aggregate.throughputMbps
                                     : 0;
                gain.Set("throughput_gain", g, 3);
                gain.Set("measured", g, 3);
                Compare(gain,
                        g,
                        phyRate > 0 ? ofdmaPhyBound / phyRate : 0,
                        0.0,
                        "khorov2019",
                        CheckKind::INFO);
            }
            else
            {
                const double g = mu.aggregate.p95LatencyMs > 0
                                     ? su.aggregate.p95LatencyMs / mu.aggregate.p95LatencyMs
                                     : 0;
                gain.Set("p95_latency_gain", g, 3);
                gain.Set("measured", g, 3);
                Compare(gain, g, 1.0, 0.0, "khorov2019", CheckKind::INFO);
            }
            table.AddRow(gain);
        }
    }

    table.Print();
    table.Write();
    const auto fails = table.PrintSummary();
    return fails ? 1 : 0;
}
