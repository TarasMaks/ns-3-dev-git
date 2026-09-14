/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file
 * @ingroup wifi-benchmarks
 *
 * TID-to-link mapping (802.11be Clause 35.3.7): traffic separation across the links of an MLD.
 *
 * An AP MLD serves N MLDs over two links with a saturated best-effort (AC_BE) downlink flow and a
 * voice-like (AC_VO) Poisson flow per station. With the default mapping all TIDs may use both
 * links, so voice frames wait behind the long best-effort A-MPDUs; with a mapping that steers
 * AC_VO to the second link and the other ACs to the first link, the voice latency is isolated
 * from the best-effort load (Lopez-Raventos and Bellalta 2022 discuss traffic-to-link allocation).
 * Association is dynamic so that the mapping is negotiated.
 *
 * Example: ./ns3 run "wifi-bench-tid-to-link --nStations=2"
 */

#include "ns3/command-line.h"
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

/// Outcome of one run
struct RunResult
{
    FlowStats vo;                    ///< voice flows
    FlowStats be;                    ///< best-effort flows
    std::vector<double> linkAirtime; ///< AP data airtime fraction per link
};

/**
 * Run one configuration.
 * @param links the links
 * @param mapped whether AC_VO is mapped to link 1 and the other ACs to link 0
 * @param nStations the number of stations
 * @param mcs the MCS
 * @param voKbps the voice load per station
 * @param voPacket the voice packet size
 * @param simTime the measurement duration
 * @param verbose print details
 * @return the run result
 */
RunResult
Run(const std::vector<LinkConfig>& links,
    bool mapped,
    uint32_t nStations,
    uint8_t mcs,
    double voKbps,
    uint32_t voPacket,
    Time simTime,
    bool verbose)
{
    NodeContainer apNode;
    apNode.Create(1);
    NodeContainer staNodes;
    staNodes.Create(nStations);
    BssBuilder builder;
    RateConfig rate;
    rate.mcs = mcs;
    MacConfig mac;
    mac.staticSetup = false;
    builder.SetStandard(WIFI_STANDARD_80211be).SetLinks(links).SetRate(rate).SetMac(mac);
    for (const auto& link : links)
    {
        builder.SetChannel(link.band, CreateSpectrumChannel(link.band));
    }
    if (mapped)
    {
        builder.SetTidToLinkMapping("0,1,2,3,4,5 0; 6,7 1", "0,1,2,3,4,5 0; 6,7 1");
    }
    Bss bss = builder.Build(apNode, staNodes);
    PlaceOnCircle(bss, 2.0);
    InstallInternet(bss, 1);
    PopulateArpCaches();

    TxAccounting txAccounting;
    txAccounting.Enable(bss.apDevice);
    FlowSet voFlows;
    FlowSet beFlows;
    const Time start = Seconds(1);
    const Time mStart = Seconds(2);
    voFlows.SetMeasurementWindow(mStart, mStart + simTime);
    beFlows.SetMeasurementWindow(mStart, mStart + simTime);
    txAccounting.SetWindow(mStart, mStart + simTime);
    double sumPhy = 0;
    for (const auto& link : links)
    {
        sumPhy += GetStandardDataRateMbps(mcs, link.width, 800, 1);
    }
    for (uint32_t i = 0; i < nStations; ++i)
    {
        voFlows.AddUdpFlow("vo" + std::to_string(i),
                           apNode.Get(0),
                           staNodes.Get(i),
                           bss.staIfs.GetAddress(i),
                           voKbps / 1000.0,
                           voPacket,
                           start,
                           mStart + simTime + MilliSeconds(1),
                           0xc0,
                           TrafficPattern::POISSON);
        beFlows.AddUdpFlow("be" + std::to_string(i),
                           apNode.Get(0),
                           staNodes.Get(i),
                           bss.staIfs.GetAddress(i),
                           sumPhy * 1.2 / nStations,
                           1472,
                           start,
                           mStart + simTime + MilliSeconds(1),
                           0x00);
    }
    Simulator::Stop(mStart + simTime + MilliSeconds(1));
    Simulator::Run();
    RunResult res;
    res.vo = voFlows.GetAggregateStats();
    res.be = beFlows.GetAggregateStats();
    res.linkAirtime.assign(links.size(), 0);
    for (const auto& r : txAccounting.GetRecords())
    {
        if (r.linkId < links.size() && r.preamble == "EHT_MU")
        {
            res.linkAirtime[r.linkId] += r.durationUs / simTime.GetMicroSeconds();
        }
    }
    if (verbose)
    {
        std::cout << "  " << (mapped ? "mapped" : "default") << ": " << txAccounting.Summary()
                  << std::endl;
    }
    Simulator::Destroy();
    return res;
}

int
main(int argc, char* argv[])
{
    CommonArgs common;
    std::string linksStr{"5:80,6:80"};
    uint32_t mcs{7};
    uint32_t nStations{2};
    double voKbps{100};
    uint32_t voPacket{200};
    Time simTime{Seconds(2)};

    CommandLine cmd(__FILE__);
    AddCommonArgs(cmd, common);
    cmd.AddValue("links", "Two band:width links", linksStr);
    cmd.AddValue("mcs", "EHT MCS index", mcs);
    cmd.AddValue("nStations", "Number of stations", nStations);
    cmd.AddValue("voiceKbps", "Voice (AC_VO) load per station in kb/s", voKbps);
    cmd.AddValue("voicePacketSize", "Voice packet size in bytes", voPacket);
    cmd.AddValue("simulationTime", "Measurement duration of each simulation", simTime);
    cmd.Parse(argc, argv);
    ApplyCommonArgs(common);

    const auto links = ParseLinks(linksStr);
    NS_ABORT_MSG_IF(links.size() != 2, "Exactly two links are expected");
    if (common.quick)
    {
        simTime = std::min(simTime, MilliSeconds(500));
    }

    PrintBanner("TID-to-link mapping: 802.11be (Wi-Fi 7)",
                "Voice latency with saturated best-effort traffic: all TIDs on both links vs "
                "AC_VO mapped to link 1 and the other ACs to link 0.",
                {"ieee80211be", "lopezraventos2022", "khorov2020"});

    ResultTable table("wifi7-tid-to-link", common.outputDir);
    RunResult def;
    for (const bool mapped : {false, true})
    {
        const auto r =
            Run(links, mapped, nStations, mcs, voKbps, voPacket, simTime, common.verbose);
        if (!mapped)
        {
            def = r;
        }
        ResultRow row;
        row.Set("mapping", mapped ? "VO->link1,others->link0" : "default-all-links");
        row.Set("n_stations", nStations);
        row.Set("mcs", mcs);
        row.Set("be_throughput_mbps", r.be.throughputMbps, 2);
        row.Set("vo_throughput_kbps", r.vo.throughputMbps * 1000, 1);
        row.Set("vo_loss_ratio", r.vo.lossRatio, 4);
        row.Set("vo_latency_p50_ms", r.vo.p50LatencyMs, 3);
        row.Set("vo_latency_p95_ms", r.vo.p95LatencyMs, 3);
        row.Set("vo_latency_p99_ms", r.vo.p99LatencyMs, 3);
        row.Set("vo_latency_max_ms", r.vo.maxLatencyMs, 3);
        for (std::size_t i = 0; i < r.linkAirtime.size(); ++i)
        {
            row.Set("ap_data_airtime_link" + std::to_string(i), r.linkAirtime[i], 3);
        }
        row.Set("measured", r.vo.p95LatencyMs, 3);
        Compare(row,
                r.vo.p95LatencyMs,
                def.vo.p95LatencyMs,
                0.0,
                "lopezraventos2022",
                mapped ? CheckKind::AT_MOST : CheckKind::INFO);
        table.AddRow(row);
    }

    table.Print();
    table.Write();
    const auto fails = table.PrintSummary();
    return fails ? 1 : 0;
}
