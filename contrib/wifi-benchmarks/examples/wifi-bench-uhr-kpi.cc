/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file
 * @ingroup wifi-benchmarks
 *
 * Ultra High Reliability (802.11bn, Wi-Fi 8) key performance indicators in a dense deployment.
 *
 * The P802.11bn Project Authorization Request defines its objectives relative to an 802.11be
 * baseline: at least 25% higher throughput at the 5th percentile, at least 25% lower latency at
 * the 95th percentile and at least 25% lower MPDU loss due to mobility (Galati-Giordano et al.
 * 2024, Reshef and Cordeiro 2022). ns-3 does not model 802.11bn; this benchmark produces the
 * baseline values of those KPIs in a dense multi-BSS scenario (a grid of co-channel APs with
 * several stations each, downlink best-effort load and uplink voice-like traffic, in the spirit of
 * the TGax simulation scenarios) for Wi-Fi 6, Wi-Fi 7 (MLO) and Wi-Fi 7 with spatial reuse, and
 * reports for each configuration the distance to the UHR objectives.
 *
 * Example: ./ns3 run "wifi-bench-uhr-kpi --nBss=4 --nStations=4 --loadPerStaMbps=20"
 */

#include "ns3/command-line.h"
#include "ns3/simulator.h"
#include "ns3/wifi-benchmark-helper.h"
#include "ns3/wifi-literature-reference.h"

#include <cmath>
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

/// KPIs of one run
struct Kpis
{
    double tput5thMbps{0};    ///< 5th percentile of the per-STA DL throughput
    double tputMedianMbps{0}; ///< median per-STA DL throughput
    double aggregateMbps{0};  ///< aggregate DL throughput
    double dlP50Ms{0};        ///< DL latency P50
    double dlP95Ms{0};        ///< DL latency P95
    double dlP99Ms{0};        ///< DL latency P99
    double dlLoss{0};         ///< DL loss ratio
    double voP95Ms{0};        ///< UL voice latency P95
    double voLoss{0};         ///< UL voice loss ratio
};

/**
 * Run one configuration.
 * @param mode "wifi6", "wifi7-mlo" or "wifi7-mlo-sr"
 * @param nBss the number of BSSs (grid)
 * @param apSpacing the AP spacing
 * @param nStations the stations per BSS
 * @param width the channel width
 * @param loadPerStaMbps the DL load per station
 * @param packetSize the DL packet size
 * @param voKbps the UL voice load per station
 * @param obssPdLevel the OBSS-PD level for the SR mode
 * @param simTime the measurement duration
 * @return the KPIs
 */
Kpis
Run(const std::string& mode,
    uint32_t nBss,
    double apSpacing,
    uint32_t nStations,
    MHz_u width,
    double loadPerStaMbps,
    uint32_t packetSize,
    double voKbps,
    double obssPdLevel,
    Time simTime)
{
    const bool eht = (mode != "wifi6");
    const bool sr = (mode == "wifi7-mlo-sr");
    std::vector<LinkConfig> links;
    LinkConfig l5;
    l5.band = Band::GHZ_5;
    l5.width = width;
    links.push_back(l5);
    if (eht)
    {
        LinkConfig l6;
        l6.band = Band::GHZ_6;
        l6.width = width;
        links.push_back(l6);
    }
    std::map<Band, Ptr<SpectrumChannel>> channels;
    for (const auto& link : links)
    {
        channels[link.band] = CreateSpectrumChannel(link.band);
    }

    std::vector<Bss> bsss;
    const uint32_t cols = static_cast<uint32_t>(std::ceil(std::sqrt(nBss)));
    for (uint32_t b = 0; b < nBss; ++b)
    {
        NodeContainer apNode;
        apNode.Create(1);
        NodeContainer staNodes;
        staNodes.Create(nStations);
        BssBuilder builder;
        RateConfig rate;
        rate.mcs = -1;
        rate.manager = "ns3::IdealWifiManager";
        builder.SetStandard(eht ? WIFI_STANDARD_80211be : WIFI_STANDARD_80211ax)
            .SetLinks(links)
            .SetRate(rate)
            .SetSsid("bss-" + std::to_string(b + 1));
        for (const auto& [band, ch] : channels)
        {
            builder.SetChannel(band, ch);
        }
        if (sr)
        {
            builder.SetBssColor(b + 1).SetObssPdLevel(dBm_u{obssPdLevel});
        }
        Bss bss = builder.Build(apNode, staNodes);
        PlaceOnCircle(bss, 5.0, Vector((b % cols) * apSpacing, (b / cols) * apSpacing, 0));
        InstallInternet(bss, b + 1);
        bsss.push_back(bss);
    }
    PopulateArpCaches();

    FlowSet dlFlows;
    FlowSet voFlows;
    const Time start = MilliSeconds(100);
    const Time mStart = start + MilliSeconds(500);
    dlFlows.SetMeasurementWindow(mStart, mStart + simTime);
    voFlows.SetMeasurementWindow(mStart, mStart + simTime);
    for (uint32_t b = 0; b < nBss; ++b)
    {
        auto& bss = bsss[b];
        for (uint32_t i = 0; i < nStations; ++i)
        {
            dlFlows.AddUdpFlow("dl-b" + std::to_string(b) + "-s" + std::to_string(i),
                               bss.apNode.Get(0),
                               bss.staNodes.Get(i),
                               bss.staIfs.GetAddress(i),
                               loadPerStaMbps,
                               packetSize,
                               start,
                               mStart + simTime + MilliSeconds(1),
                               0,
                               TrafficPattern::POISSON);
            voFlows.AddUdpFlow("vo-b" + std::to_string(b) + "-s" + std::to_string(i),
                               bss.staNodes.Get(i),
                               bss.apNode.Get(0),
                               bss.apIf.GetAddress(0),
                               voKbps / 1000.0,
                               200,
                               start,
                               mStart + simTime + MilliSeconds(1),
                               0xc0,
                               TrafficPattern::POISSON);
        }
    }
    Simulator::Stop(mStart + simTime + MilliSeconds(1));
    Simulator::Run();
    Kpis k;
    const auto dl = dlFlows.GetAggregateStats();
    const auto vo = voFlows.GetAggregateStats();
    k.tput5thMbps = dlFlows.GetThroughputPercentile(5);
    k.tputMedianMbps = dlFlows.GetThroughputPercentile(50);
    k.aggregateMbps = dl.throughputMbps;
    k.dlP50Ms = dl.p50LatencyMs;
    k.dlP95Ms = dl.p95LatencyMs;
    k.dlP99Ms = dl.p99LatencyMs;
    k.dlLoss = dl.lossRatio;
    k.voP95Ms = vo.p95LatencyMs;
    k.voLoss = vo.lossRatio;
    Simulator::Destroy();
    return k;
}

int
main(int argc, char* argv[])
{
    CommonArgs common;
    uint32_t nBss{4};
    double apSpacing{15};
    uint32_t nStations{4};
    double channelWidthMhz{80};
    double loadPerStaMbps{20};
    uint32_t packetSize{1200};
    double voKbps{100};
    double obssPdLevel{-72};
    std::string modesStr{"wifi6,wifi7-mlo,wifi7-mlo-sr"};
    Time simTime{Seconds(3)};

    CommandLine cmd(__FILE__);
    AddCommonArgs(cmd, common);
    cmd.AddValue("nBss", "Number of co-channel BSSs (placed on a grid)", nBss);
    cmd.AddValue("apSpacing", "Distance between neighboring APs (m)", apSpacing);
    cmd.AddValue("nStations", "Stations per BSS", nStations);
    cmd.AddValue("channelWidth", "Channel width in MHz (all links)", channelWidthMhz);
    cmd.AddValue("loadPerStaMbps",
                 "DL best-effort Poisson load per station (Mb/s)",
                 loadPerStaMbps);
    cmd.AddValue("packetSize", "DL packet size in bytes", packetSize);
    cmd.AddValue("voiceKbps", "UL voice-like load per station (kb/s, 200-byte packets)", voKbps);
    cmd.AddValue("obssPdLevel", "OBSS-PD level of the spatial reuse mode (dBm)", obssPdLevel);
    cmd.AddValue("modes", "Comma separated list among wifi6, wifi7-mlo, wifi7-mlo-sr", modesStr);
    cmd.AddValue("simulationTime", "Measurement duration of each simulation", simTime);
    cmd.Parse(argc, argv);
    ApplyCommonArgs(common);

    auto modes = Split(modesStr);
    if (common.quick)
    {
        nBss = std::min<uint32_t>(nBss, 2);
        nStations = std::min<uint32_t>(nStations, 2);
        simTime = std::min(simTime, MilliSeconds(500));
    }

    PrintBanner("UHR (802.11bn, Wi-Fi 8) KPI baselines in a dense deployment",
                "5th-percentile throughput, P95 latency and loss of co-channel BSSs for Wi-Fi 6, "
                "Wi-Fi 7 MLO and Wi-Fi 7 MLO with spatial reuse, vs the +25%/-25% objectives of "
                "the P802.11bn PAR relative to the 802.11be baseline.",
                {"ieee80211bn-par", "galati2024", "reshef2022", "tgax-scenarios"});

    ResultTable table("wifi8-uhr-kpi", common.outputDir);
    const MHz_u width{channelWidthMhz};
    std::map<std::string, Kpis> results;
    for (const auto& mode : modes)
    {
        results[mode] = Run(mode,
                            nBss,
                            apSpacing,
                            nStations,
                            width,
                            loadPerStaMbps,
                            packetSize,
                            voKbps,
                            obssPdLevel,
                            simTime);
    }
    const bool haveBaseline = results.count("wifi7-mlo") > 0;
    const Kpis baseline = haveBaseline ? results["wifi7-mlo"] : Kpis();
    for (const auto& mode : modes)
    {
        const auto& k = results[mode];
        ResultRow row;
        row.Set("mode", mode);
        row.Set("n_bss", nBss);
        row.Set("stations_per_bss", nStations);
        row.Set("width_mhz", channelWidthMhz, 0);
        row.Set("dl_load_per_sta_mbps", loadPerStaMbps, 1);
        row.Set("aggregate_dl_mbps", k.aggregateMbps, 2);
        row.Set("dl_tput_5th_pct_mbps", k.tput5thMbps, 3);
        row.Set("dl_tput_median_mbps", k.tputMedianMbps, 3);
        row.Set("dl_latency_p50_ms", k.dlP50Ms, 3);
        row.Set("dl_latency_p95_ms", k.dlP95Ms, 3);
        row.Set("dl_latency_p99_ms", k.dlP99Ms, 3);
        row.Set("dl_loss_ratio", k.dlLoss, 4);
        row.Set("ul_voice_p95_ms", k.voP95Ms, 3);
        row.Set("ul_voice_loss_ratio", k.voLoss, 4);
        if (haveBaseline)
        {
            const double tputTarget =
                baseline.tput5thMbps * (1 + UHR_5TH_PERCENTILE_THROUGHPUT_GAIN);
            const double latTarget = baseline.dlP95Ms * (1 - UHR_P95_LATENCY_REDUCTION);
            row.Set("uhr_5th_pct_tput_target_mbps", tputTarget, 3);
            row.Set("uhr_p95_latency_target_ms", latTarget, 3);
            row.Set("meets_uhr_tput_objective", k.tput5thMbps >= tputTarget ? "yes" : "no");
            row.Set("meets_uhr_latency_objective", k.dlP95Ms <= latTarget ? "yes" : "no");
        }
        row.Set("measured", k.tput5thMbps, 3);
        if (mode == "wifi7-mlo" && results.count("wifi6"))
        {
            // sanity: the dual-link 802.11be baseline should not be worse than Wi-Fi 6
            Compare(row,
                    k.tput5thMbps,
                    results["wifi6"].tput5thMbps,
                    0.1,
                    "galati2024",
                    CheckKind::AT_LEAST);
        }
        else
        {
            Compare(row,
                    k.tput5thMbps,
                    haveBaseline ? baseline.tput5thMbps : k.tput5thMbps,
                    0.0,
                    "ieee80211bn-par",
                    CheckKind::INFO);
        }
        table.AddRow(row);
    }

    table.Print();
    table.Write();
    const auto fails = table.PrintSummary();
    return fails ? 1 : 0;
}
