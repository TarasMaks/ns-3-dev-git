/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file
 * @ingroup wifi-benchmarks
 *
 * Multi-link operation (802.11be) latency under asymmetric contention.
 *
 * An AP MLD serves N stations with Poisson downlink traffic while an overlapping BSS saturates
 * the channel of the first link. Three configurations are compared: single-link devices on the
 * contended link, single-link devices on the idle link, and MLDs using both links. The literature
 * (Carrascosa-Zamacois et al. 2023, Lopez-Raventos and Bellalta 2022) reports that MLO reduces
 * the latency, in particular its tail, by steering traffic to the link that is available first.
 *
 * Example: ./ns3 run "wifi-bench-mlo-latency --loadMbps=40 --nStations=4"
 */

#include "ns3/command-line.h"
#include "ns3/mobility-helper.h"
#include "ns3/simulator.h"
#include "ns3/wifi-benchmark-helper.h"
#include "ns3/wifi-literature-reference.h"

#include <sstream>

using namespace ns3;
using namespace ns3::wifibench;

/**
 * Parse a link list such as "5:80,6:80".
 * @param s the string
 * @return the links
 */
std::vector<LinkConfig>
ParseLinks(const std::string& s)
{
    std::vector<LinkConfig> links;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ','))
    {
        const auto colon = item.find(':');
        NS_ABORT_MSG_IF(colon == std::string::npos, "Invalid link spec " << item);
        LinkConfig link;
        link.band = BandFromGhz(std::stod(item.substr(0, colon)));
        link.width = MHz_u{std::stod(item.substr(colon + 1))};
        links.push_back(link);
    }
    return links;
}

/**
 * Run one configuration.
 * @param links the links of the main BSS
 * @param obssLink the link (band/width) of the overlapping BSS
 * @param nStations the number of stations
 * @param mcs the MCS
 * @param loadMbps the aggregate DL offered load of the main BSS
 * @param packetSize the packet size
 * @param obssSaturated whether the OBSS is saturated
 * @param simTime the measurement duration
 * @param verbose print details
 * @return the aggregate flow statistics of the main BSS
 */
FlowStats
Run(const std::vector<LinkConfig>& links,
    const LinkConfig& obssLink,
    uint32_t nStations,
    uint8_t mcs,
    double loadMbps,
    uint32_t packetSize,
    bool obssSaturated,
    Time simTime,
    bool verbose)
{
    std::map<Band, Ptr<SpectrumChannel>> channels;
    for (const auto& link : links)
    {
        if (!channels.count(link.band))
        {
            channels[link.band] = CreateSpectrumChannel(link.band);
        }
    }
    if (!channels.count(obssLink.band))
    {
        channels[obssLink.band] = CreateSpectrumChannel(obssLink.band);
    }

    NodeContainer apNode;
    apNode.Create(1);
    NodeContainer staNodes;
    staNodes.Create(nStations);
    BssBuilder builder;
    RateConfig rate;
    rate.mcs = mcs;
    builder.SetStandard(WIFI_STANDARD_80211be).SetLinks(links).SetRate(rate).SetSsid("main");
    for (const auto& [band, ch] : channels)
    {
        builder.SetChannel(band, ch);
    }
    Bss bss = builder.Build(apNode, staNodes);
    PlaceOnCircle(bss, 5.0);
    InstallInternet(bss, 1);

    NodeContainer obssApNode;
    obssApNode.Create(1);
    NodeContainer obssStaNodes;
    obssStaNodes.Create(1);
    BssBuilder obssBuilder;
    obssBuilder.SetStandard(WIFI_STANDARD_80211be)
        .AddLink(obssLink)
        .SetRate(rate)
        .SetSsid("obss")
        .SetChannel(obssLink.band, channels[obssLink.band]);
    Bss obss = obssBuilder.Build(obssApNode, obssStaNodes);
    PlaceOnCircle(obss, 5.0, Vector(20, 0, 0));
    InstallInternet(obss, 2);
    PopulateArpCaches();

    FlowSet flows;
    const Time start = MilliSeconds(100);
    const Time mStart = start + MilliSeconds(300);
    flows.SetMeasurementWindow(mStart, mStart + simTime);
    for (uint32_t i = 0; i < nStations; ++i)
    {
        flows.AddUdpFlow("dl" + std::to_string(i),
                         apNode.Get(0),
                         staNodes.Get(i),
                         bss.staIfs.GetAddress(i),
                         loadMbps / nStations,
                         packetSize,
                         start,
                         mStart + simTime + MilliSeconds(1),
                         0,
                         TrafficPattern::POISSON);
    }
    FlowSet obssFlows;
    obssFlows.SetMeasurementWindow(mStart, mStart + simTime);
    if (obssSaturated)
    {
        const double obssPhy = GetStandardDataRateMbps(mcs, obssLink.width, 800, 1);
        obssFlows.AddUdpFlow("obss-ul",
                             obssStaNodes.Get(0),
                             obssApNode.Get(0),
                             obss.apIf.GetAddress(0),
                             obssPhy * 1.2,
                             1472,
                             start,
                             mStart + simTime + MilliSeconds(1));
    }
    Simulator::Stop(mStart + simTime + MilliSeconds(1));
    Simulator::Run();
    const auto stats = flows.GetAggregateStats();
    if (verbose)
    {
        std::cout << "  OBSS throughput: " << obssFlows.GetAggregateStats().throughputMbps
                  << " Mb/s" << std::endl;
    }
    Simulator::Destroy();
    return stats;
}

int
main(int argc, char* argv[])
{
    CommonArgs common;
    std::string linksStr{"5:80,6:80"};
    uint32_t mcs{7};
    uint32_t nStations{2};
    double loadMbps{20};
    uint32_t packetSize{500};
    Time simTime{Seconds(2)};

    CommandLine cmd(__FILE__);
    AddCommonArgs(cmd, common);
    cmd.AddValue("links", "Two band:width links (the OBSS occupies the first one)", linksStr);
    cmd.AddValue("mcs", "EHT MCS index", mcs);
    cmd.AddValue("nStations", "Number of stations of the main BSS", nStations);
    cmd.AddValue("loadMbps", "Aggregate DL offered load of the main BSS (Poisson)", loadMbps);
    cmd.AddValue("packetSize", "Packet size in bytes", packetSize);
    cmd.AddValue("simulationTime", "Measurement duration of each simulation", simTime);
    cmd.Parse(argc, argv);
    ApplyCommonArgs(common);

    const auto links = ParseLinks(linksStr);
    NS_ABORT_MSG_IF(links.size() != 2, "Exactly two links are expected");
    if (common.quick)
    {
        simTime = std::min(simTime, MilliSeconds(500));
    }

    PrintBanner("Multi-link operation latency under asymmetric contention: 802.11be (Wi-Fi 7)",
                "Poisson DL traffic with an overlapping BSS saturating link 0: single-link on the "
                "contended link vs single-link on the idle link vs MLO.",
                {"ieee80211be", "carrascosa2023", "lopezraventos2022"});

    ResultTable table("wifi7-mlo-latency", common.outputDir);

    struct Mode
    {
        std::string name;
        std::vector<LinkConfig> links;
    };

    std::vector<Mode> modes{{"sld-contended-link", {links[0]}},
                            {"sld-idle-link", {links[1]}},
                            {"mlo-both-links", links}};
    FlowStats contended;
    for (const auto& mode : modes)
    {
        const auto stats = Run(mode.links,
                               links[0],
                               nStations,
                               mcs,
                               loadMbps,
                               packetSize,
                               true,
                               simTime,
                               common.verbose);
        if (mode.name == "sld-contended-link")
        {
            contended = stats;
        }
        ResultRow row;
        row.Set("mode", mode.name);
        row.Set("n_links", static_cast<uint32_t>(mode.links.size()));
        row.Set("n_stations", nStations);
        row.Set("load_mbps", loadMbps, 1);
        row.Set("throughput_mbps", stats.throughputMbps, 2);
        row.Set("loss_ratio", stats.lossRatio, 4);
        row.Set("latency_p50_ms", stats.p50LatencyMs, 3);
        row.Set("latency_p95_ms", stats.p95LatencyMs, 3);
        row.Set("latency_p99_ms", stats.p99LatencyMs, 3);
        row.Set("latency_max_ms", stats.maxLatencyMs, 3);
        row.Set("p95_gain_vs_contended",
                stats.p95LatencyMs > 0 ? contended.p95LatencyMs / stats.p95LatencyMs : 0,
                3);
        row.Set("measured", stats.p95LatencyMs, 3);
        Compare(row,
                stats.p95LatencyMs,
                contended.p95LatencyMs,
                0.0,
                "carrascosa2023",
                mode.name == "mlo-both-links" ? CheckKind::AT_MOST : CheckKind::INFO);
        table.AddRow(row);
    }

    table.Print();
    table.Write();
    const auto fails = table.PrintSummary();
    return fails ? 1 : 0;
}
