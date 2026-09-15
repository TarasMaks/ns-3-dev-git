/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file
 * @ingroup wifi-benchmarks
 *
 * Multi-AP coordination (802.11bn, Wi-Fi 8) emulation: uncoordinated access vs coordinated TDMA
 * vs coordinated spatial reuse between two overlapping BSSs.
 *
 * Multi-AP coordination is a candidate feature of 802.11bn (Galati-Giordano et al. 2024,
 * Nunez et al. 2022) that ns-3 does not model. This benchmark emulates two of its simplest forms
 * on top of the existing MAC: coordinated TDMA (Co-TDMA), emulated by alternately blocking the
 * downlink queues of the two APs in fixed slots so that only one AP contends at a time, and
 * coordinated spatial reuse (Co-SR), approximated by the 802.11ax OBSS-PD mechanism with BSS
 * coloring. The 5th-percentile station throughput and the P95 latency of Poisson downlink traffic
 * are compared with the uncoordinated baseline and with the +25%/-25% objectives of the P802.11bn
 * PAR.
 *
 * Example: ./ns3 run "wifi-bench-multi-ap --loadPerStaMbps=30 --slot=5ms"
 */

#include "ns3/ap-wifi-mac.h"
#include "ns3/command-line.h"
#include "ns3/simulator.h"
#include "ns3/wifi-benchmark-helper.h"
#include "ns3/wifi-literature-reference.h"
#include "ns3/wifi-mac-queue-scheduler.h"

#include <sstream>

using namespace ns3;
using namespace ns3::wifibench;

/**
 * @param s a comma separated list
 * @return the items
 */
std::vector<std::string>
Split(const std::string& s)
{
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ','))
    {
        if (!item.empty())
        {
            out.push_back(item);
        }
    }
    return out;
}

/**
 * Block or unblock the downlink data queues of an AP towards all its stations (Co-TDMA
 * emulation).
 * @param bss the BSS
 * @param blocked whether to block
 */
void
SetApBlocked(const Bss& bss, bool blocked)
{
    auto apMac = bss.ApMac();
    auto scheduler = apMac->GetMacQueueScheduler();
    const auto apAddr = apMac->GetAddress();
    for (uint32_t i = 0; i < bss.staNodes.GetN(); ++i)
    {
        const auto staAddr = bss.StaDev(i)->GetMac()->GetAddress();
        // TID_NOT_MAPPED is a reason that no entity of this scenario manipulates
        if (blocked)
        {
            scheduler->BlockQueues(WifiQueueBlockedReason::TID_NOT_MAPPED,
                                   AC_BE,
                                   {WIFI_QOSDATA_QUEUE},
                                   staAddr,
                                   apAddr);
        }
        else
        {
            scheduler->UnblockQueues(WifiQueueBlockedReason::TID_NOT_MAPPED,
                                     AC_BE,
                                     {WIFI_QOSDATA_QUEUE},
                                     staAddr,
                                     apAddr);
        }
    }
}

/// KPIs of one run
struct Kpis
{
    double tput5thMbps{0};   ///< 5th percentile of the per-STA throughput
    double aggregateMbps{0}; ///< aggregate throughput
    double p50Ms{0};         ///< latency P50
    double p95Ms{0};         ///< latency P95
    double p99Ms{0};         ///< latency P99
    double loss{0};          ///< loss ratio
    double tputBss1{0};      ///< BSS 1 throughput
    double tputBss2{0};      ///< BSS 2 throughput
};

/**
 * Run one configuration.
 * @param mode "uncoordinated", "co-tdma" or "co-sr"
 * @param apDistance the AP-AP distance
 * @param nStations the stations per BSS
 * @param width the channel width
 * @param loadPerStaMbps the DL load per station
 * @param packetSize the packet size
 * @param slot the Co-TDMA slot duration
 * @param obssPdLevel the OBSS-PD level of the Co-SR mode
 * @param simTime the measurement duration
 * @return the KPIs
 */
Kpis
Run(const std::string& mode,
    double apDistance,
    uint32_t nStations,
    MHz_u width,
    double loadPerStaMbps,
    uint32_t packetSize,
    Time slot,
    double obssPdLevel,
    Time simTime)
{
    const Band band = Band::GHZ_5;
    auto channel = CreateSpectrumChannel(band);
    std::vector<Bss> bsss;
    for (uint8_t b = 0; b < 2; ++b)
    {
        NodeContainer apNode;
        apNode.Create(1);
        NodeContainer staNodes;
        staNodes.Create(nStations);
        BssBuilder builder;
        LinkConfig link;
        link.band = band;
        link.width = width;
        RateConfig rate;
        rate.mcs = -1;
        rate.manager = "ns3::IdealWifiManager";
        builder.SetStandard(WIFI_STANDARD_80211be)
            .AddLink(link)
            .SetRate(rate)
            .SetSsid("bss-" + std::to_string(b + 1))
            .SetChannel(band, channel);
        if (mode == "co-sr")
        {
            builder.SetBssColor(b + 1).SetObssPdLevel(dBm_u{obssPdLevel});
        }
        Bss bss = builder.Build(apNode, staNodes);
        PlaceOnCircle(bss, 5.0, Vector(b * apDistance, 0, 0));
        InstallInternet(bss, b + 1);
        bsss.push_back(bss);
    }
    PopulateArpCaches();

    FlowSet flows;
    const Time start = MilliSeconds(100);
    const Time mStart = start + MilliSeconds(500);
    const Time end = mStart + simTime + MilliSeconds(1);
    flows.SetMeasurementWindow(mStart, mStart + simTime);
    std::vector<uint32_t> flowIds[2];
    for (uint8_t b = 0; b < 2; ++b)
    {
        auto& bss = bsss[b];
        for (uint32_t i = 0; i < nStations; ++i)
        {
            flowIds[b].push_back(
                flows.AddUdpFlow("dl-b" + std::to_string(b) + "-s" + std::to_string(i),
                                 bss.apNode.Get(0),
                                 bss.staNodes.Get(i),
                                 bss.staIfs.GetAddress(i),
                                 loadPerStaMbps,
                                 packetSize,
                                 start,
                                 end,
                                 0,
                                 TrafficPattern::POISSON));
        }
    }
    if (mode == "co-tdma")
    {
        uint32_t k = 0;
        for (Time t = start; t < end; t += slot, ++k)
        {
            const uint8_t owner = k % 2;
            Simulator::Schedule(t, [&bsss, owner]() {
                SetApBlocked(bsss[owner], false);
                SetApBlocked(bsss[1 - owner], true);
            });
        }
    }
    Simulator::Stop(end);
    Simulator::Run();
    Kpis k;
    const auto agg = flows.GetAggregateStats();
    k.tput5thMbps = flows.GetThroughputPercentile(5);
    k.aggregateMbps = agg.throughputMbps;
    k.p50Ms = agg.p50LatencyMs;
    k.p95Ms = agg.p95LatencyMs;
    k.p99Ms = agg.p99LatencyMs;
    k.loss = agg.lossRatio;
    for (auto id : flowIds[0])
    {
        k.tputBss1 += flows.GetStats(id).throughputMbps;
    }
    for (auto id : flowIds[1])
    {
        k.tputBss2 += flows.GetStats(id).throughputMbps;
    }
    Simulator::Destroy();
    return k;
}

int
main(int argc, char* argv[])
{
    CommonArgs common;
    double apDistance{25};
    uint32_t nStations{2};
    double channelWidthMhz{20};
    double loadPerStaMbps{30};
    uint32_t packetSize{1200};
    Time slot{MilliSeconds(5)};
    double obssPdLevel{-72};
    std::string modesStr{"uncoordinated,co-tdma,co-sr"};
    Time simTime{Seconds(3)};

    CommandLine cmd(__FILE__);
    AddCommonArgs(cmd, common);
    cmd.AddValue("apDistance", "Distance between the two APs (m)", apDistance);
    cmd.AddValue("nStations", "Stations per BSS", nStations);
    cmd.AddValue("channelWidth", "Channel width in MHz", channelWidthMhz);
    cmd.AddValue("loadPerStaMbps", "DL Poisson load per station (Mb/s)", loadPerStaMbps);
    cmd.AddValue("packetSize", "Packet size in bytes", packetSize);
    cmd.AddValue("slot", "Co-TDMA slot duration", slot);
    cmd.AddValue("obssPdLevel", "OBSS-PD level of the Co-SR mode (dBm)", obssPdLevel);
    cmd.AddValue("modes", "Comma separated list among uncoordinated, co-tdma, co-sr", modesStr);
    cmd.AddValue("simulationTime", "Measurement duration of each simulation", simTime);
    cmd.Parse(argc, argv);
    ApplyCommonArgs(common);

    auto modes = Split(modesStr);
    if (common.quick)
    {
        simTime = std::min(simTime, MilliSeconds(500));
    }

    PrintBanner("Multi-AP coordination emulation (802.11bn, Wi-Fi 8)",
                "Two overlapping BSSs with Poisson DL traffic: uncoordinated vs emulated "
                "coordinated TDMA vs coordinated spatial reuse (OBSS-PD proxy); 5th-percentile "
                "throughput and P95 latency vs the P802.11bn PAR objectives.",
                {"ieee80211bn-par", "galati2024", "nunez2022", "reshef2022"});

    ResultTable table("wifi8-multi-ap", common.outputDir);
    const MHz_u width{channelWidthMhz};
    std::map<std::string, Kpis> results;
    for (const auto& mode : modes)
    {
        results[mode] = Run(mode,
                            apDistance,
                            nStations,
                            width,
                            loadPerStaMbps,
                            packetSize,
                            slot,
                            obssPdLevel,
                            simTime);
    }
    const bool haveBaseline = results.count("uncoordinated") > 0;
    const Kpis baseline = haveBaseline ? results["uncoordinated"] : Kpis();
    for (const auto& mode : modes)
    {
        const auto& k = results[mode];
        ResultRow row;
        row.Set("mode", mode);
        row.Set("ap_distance_m", apDistance, 1);
        row.Set("stations_per_bss", nStations);
        row.Set("width_mhz", channelWidthMhz, 0);
        row.Set("load_per_sta_mbps", loadPerStaMbps, 1);
        row.Set("tdma_slot_ms", mode == "co-tdma" ? slot.GetMicroSeconds() / 1000.0 : 0.0, 1);
        row.Set("aggregate_mbps", k.aggregateMbps, 2);
        row.Set("tput_bss1_mbps", k.tputBss1, 2);
        row.Set("tput_bss2_mbps", k.tputBss2, 2);
        row.Set("tput_5th_pct_mbps", k.tput5thMbps, 3);
        row.Set("latency_p50_ms", k.p50Ms, 3);
        row.Set("latency_p95_ms", k.p95Ms, 3);
        row.Set("latency_p99_ms", k.p99Ms, 3);
        row.Set("loss_ratio", k.loss, 4);
        if (haveBaseline)
        {
            const double tputGain =
                baseline.tput5thMbps > 0 ? k.tput5thMbps / baseline.tput5thMbps - 1 : 0;
            const double latReduction = baseline.p95Ms > 0 ? 1 - k.p95Ms / baseline.p95Ms : 0;
            row.Set("tput_5th_pct_gain", tputGain, 3);
            row.Set("p95_latency_reduction", latReduction, 3);
            row.Set("meets_uhr_tput_objective",
                    tputGain >= UHR_5TH_PERCENTILE_THROUGHPUT_GAIN ? "yes" : "no");
            row.Set("meets_uhr_latency_objective",
                    latReduction >= UHR_P95_LATENCY_REDUCTION ? "yes" : "no");
        }
        row.Set("measured", k.tput5thMbps, 3);
        Compare(row,
                k.tput5thMbps,
                haveBaseline ? baseline.tput5thMbps : k.tput5thMbps,
                0.0,
                "ieee80211bn-par",
                CheckKind::INFO);
        table.AddRow(row);
    }

    table.Print();
    table.Write();
    const auto fails = table.PrintSummary();
    return fails ? 1 : 0;
}
