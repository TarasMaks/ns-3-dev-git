/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file
 * @ingroup wifi-benchmarks
 *
 * Multi-link operation (802.11be, Wi-Fi 7): throughput scaling with the number of links.
 *
 * An AP MLD and N non-AP MLDs operate over 1, 2 or 3 links (by default 80 MHz in 5 GHz, 80 MHz
 * in 6 GHz and 40 MHz in 2.4 GHz). Saturated UDP traffic is sent and the aggregate goodput is
 * compared with the sum of the analytical single-link bounds; the literature reports that STR
 * (simultaneous transmit and receive) MLO aggregates the capacity of the links (Lopez-Raventos
 * and Bellalta 2022, Carrascosa-Zamacois et al. 2023, Deng et al. 2020).
 *
 * Example: ./ns3 run "wifi-bench-mlo-throughput --links=5:80,6:160 --mcs=13"
 */

#include "ns3/command-line.h"
#include "ns3/simulator.h"
#include "ns3/wifi-benchmark-helper.h"
#include "ns3/wifi-literature-reference.h"

#include <sstream>

using namespace ns3;
using namespace ns3::wifibench;

/**
 * Parse a link list such as "5:80,6:80,2.4:40".
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

int
main(int argc, char* argv[])
{
    CommonArgs common;
    std::string linksStr{"5:80,6:80,2.4:40"};
    uint32_t mcs{11};
    uint32_t nStations{1};
    bool downlink{true};
    uint32_t payload{1472};
    Time simTime{Seconds(1)};
    double tolerance{0.08};

    CommandLine cmd(__FILE__);
    AddCommonArgs(cmd, common);
    cmd.AddValue("links",
                 "Comma separated list of band:width links (e.g. 5:80,6:80,2.4:40)",
                 linksStr);
    cmd.AddValue("mcs", "EHT MCS index used on all links", mcs);
    cmd.AddValue("nStations", "Number of non-AP MLDs", nStations);
    cmd.AddValue("downlink", "Downlink traffic if true, uplink otherwise", downlink);
    cmd.AddValue("payloadSize", "UDP payload size in bytes", payload);
    cmd.AddValue("simulationTime", "Measurement duration of each simulation", simTime);
    cmd.AddValue("tolerance", "Relative tolerance vs the sum of the single-link bounds", tolerance);
    cmd.Parse(argc, argv);
    ApplyCommonArgs(common);

    const WifiStandard standard = WIFI_STANDARD_80211be;
    const auto allLinks = ParseLinks(linksStr);
    const uint16_t gi = 800;
    if (common.quick)
    {
        simTime = std::min(simTime, MilliSeconds(300));
    }

    PrintBanner("Multi-link operation throughput scaling: 802.11be (Wi-Fi 7)",
                "Aggregate saturated goodput of an AP MLD with 1..L links vs the sum of the "
                "single-link analytical bounds.",
                {"ieee80211be", "lopezraventos2022", "carrascosa2023", "deng2020"});

    ResultTable table("wifi7-mlo-throughput", common.outputDir);
    double singleLinkTput = 0;
    for (std::size_t nLinks = 1; nLinks <= allLinks.size(); ++nLinks)
    {
        std::vector<LinkConfig> links(allLinks.begin(), allLinks.begin() + nLinks);
        // analytical bound: sum over links
        double bound = 0;
        double sumPhy = 0;
        std::ostringstream linkDesc;
        for (std::size_t i = 0; i < links.size(); ++i)
        {
            const auto& link = links[i];
            const auto [k, ppduUs] = SaturatedAmpdu(GetMpduBytes(payload),
                                                    EHT_MAX_BA_WINDOW,
                                                    EHT_MAX_AMPDU_BYTES,
                                                    MAX_PPDU_DURATION_US,
                                                    standard,
                                                    mcs,
                                                    link.width,
                                                    gi,
                                                    1,
                                                    link.band);
            MacModelTiming timing;
            timing.aifsUs = SIFS_US + GetStandardEdcaParams(AC_BE).aifsn * SLOT_US;
            timing.dataPpduUs = ppduUs;
            timing.ackPpduUs = ControlPpduDurationUs(GetBlockAckBytes(EHT_MAX_BA_WINDOW),
                                                     ControlModeName(standard, link.band, mcs),
                                                     link.band);
            bound += ComputeSingleUserGoodputMbps(timing, k, payload);
            sumPhy += GetStandardDataRateMbps(mcs, link.width, gi, 1);
            linkDesc << (i ? "+" : "") << BandName(link.band) << "/" << link.width << "MHz";
        }

        NodeContainer apNode;
        apNode.Create(1);
        NodeContainer staNodes;
        staNodes.Create(nStations);
        BssBuilder builder;
        RateConfig rate;
        rate.mcs = mcs;
        MacConfig mac;
        mac.guardIntervalNs = gi;
        builder.SetStandard(standard).SetLinks(links).SetRate(rate).SetMac(mac);
        for (const auto& link : links)
        {
            builder.SetChannel(link.band, CreateSpectrumChannel(link.band));
        }
        Bss bss = builder.Build(apNode, staNodes);
        PlaceOnCircle(bss, 1.0);
        InstallInternet(bss, 1);
        PopulateArpCaches();

        TxAccounting txAccounting;
        txAccounting.Enable(bss.AllDevices());
        FlowSet flows;
        const Time start = MilliSeconds(100);
        const Time mStart = start + MilliSeconds(200);
        flows.SetMeasurementWindow(mStart, mStart + simTime);
        txAccounting.SetWindow(mStart, mStart + simTime);
        for (uint32_t i = 0; i < nStations; ++i)
        {
            if (downlink)
            {
                flows.AddUdpFlow("dl" + std::to_string(i),
                                 apNode.Get(0),
                                 staNodes.Get(i),
                                 bss.staIfs.GetAddress(i),
                                 sumPhy * 1.2 / nStations,
                                 payload,
                                 start,
                                 mStart + simTime + MilliSeconds(1));
            }
            else
            {
                flows.AddUdpFlow("ul" + std::to_string(i),
                                 staNodes.Get(i),
                                 apNode.Get(0),
                                 bss.apIf.GetAddress(0),
                                 sumPhy * 1.2 / nStations,
                                 payload,
                                 start,
                                 mStart + simTime + MilliSeconds(1));
            }
        }
        Simulator::Stop(mStart + simTime + MilliSeconds(1));
        Simulator::Run();
        const auto agg = flows.GetAggregateStats();
        std::vector<double> linkAirtime(nLinks, 0);
        for (const auto& r : txAccounting.GetRecords())
        {
            if (r.linkId < nLinks && r.preamble == "EHT_MU")
            {
                linkAirtime[r.linkId] += r.durationUs;
            }
        }
        if (common.verbose)
        {
            std::cout << "  " << nLinks << " links: " << txAccounting.Summary() << std::endl;
        }
        Simulator::Destroy();
        if (nLinks == 1)
        {
            singleLinkTput = agg.throughputMbps;
        }

        ResultRow row;
        row.Set("n_links", static_cast<uint32_t>(nLinks));
        row.Set("links", linkDesc.str());
        row.Set("mcs", mcs);
        row.Set("n_stations", nStations);
        row.Set("direction", downlink ? "DL" : "UL");
        row.Set("sum_phy_rate_mbps", sumPhy, 1);
        for (std::size_t i = 0; i < nLinks; ++i)
        {
            row.Set("data_airtime_link" + std::to_string(i),
                    linkAirtime[i] / simTime.GetMicroSeconds(),
                    3);
        }
        row.Set("gain_vs_single_link",
                singleLinkTput > 0 ? agg.throughputMbps / singleLinkTput : 0,
                3);
        row.Set("measured", agg.throughputMbps, 2);
        Compare(row, agg.throughputMbps, bound, tolerance, "lopezraventos2022", CheckKind::WITHIN);
        table.AddRow(row);
    }

    table.Print();
    table.Write();
    const auto fails = table.PrintSummary();
    return fails ? 1 : 0;
}
