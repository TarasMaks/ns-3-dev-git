/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file
 * @ingroup wifi-benchmarks
 *
 * Enhanced multi-link single radio (EMLSR, 802.11be Clause 35.3.17) operation.
 *
 * A non-AP MLD with a single full-capability radio (main PHY) and a low-capability auxiliary PHY
 * listening on the other link is compared with a single-link device and with a dual-radio (STR)
 * MLD, in two scenarios: saturated downlink (throughput, where EMLSR pays the initial control
 * frame, padding and transition overheads) and low-load downlink under contention on one link
 * (latency, where EMLSR can be served on whichever link is available). The EMLSR parameters
 * (padding delay, transition delay, auxiliary PHY width) are configurable. EMLSR support in ns-3
 * is experimental.
 *
 * Example: ./ns3 run "wifi-bench-emlsr --emlsrPaddingDelay=64us --emlsrTransitionDelay=256us"
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

/**
 * Run one configuration.
 * @param mode "sld", "mld" or "emlsr"
 * @param links the links
 * @param mcs the MCS
 * @param saturated saturated DL traffic (else low-load Poisson with an OBSS on link 0)
 * @param loadMbps the low-load offered load
 * @param packetSize the packet size
 * @param emlsr the EMLSR configuration
 * @param simTime the measurement duration
 * @param verbose print details
 * @return the flow statistics
 */
FlowStats
Run(const std::string& mode,
    const std::vector<LinkConfig>& links,
    uint8_t mcs,
    bool saturated,
    double loadMbps,
    uint32_t packetSize,
    const EmlsrConfig& emlsr,
    Time simTime,
    bool verbose)
{
    std::vector<LinkConfig> devLinks = (mode == "sld") ? std::vector<LinkConfig>{links[0]} : links;
    std::map<Band, Ptr<SpectrumChannel>> channels;
    for (const auto& link : links)
    {
        if (!channels.count(link.band))
        {
            channels[link.band] = CreateSpectrumChannel(link.band);
        }
    }

    NodeContainer apNode;
    apNode.Create(1);
    NodeContainer staNodes;
    staNodes.Create(1);
    BssBuilder builder;
    RateConfig rate;
    rate.mcs = mcs;
    MacConfig mac;
    builder.SetStandard(WIFI_STANDARD_80211be)
        .SetLinks(devLinks)
        .SetRate(rate)
        .SetMac(mac)
        .SetSsid("main");
    for (const auto& [band, ch] : channels)
    {
        builder.SetChannel(band, ch);
    }
    if (mode == "emlsr")
    {
        builder.SetEmlsr(emlsr);
    }
    Bss bss = builder.Build(apNode, staNodes);
    PlaceOnCircle(bss, 2.0);
    InstallInternet(bss, 1);

    Bss obss;
    NodeContainer obssApNode;
    NodeContainer obssStaNodes;
    if (!saturated)
    {
        obssApNode.Create(1);
        obssStaNodes.Create(1);
        BssBuilder obssBuilder;
        obssBuilder.SetStandard(WIFI_STANDARD_80211be)
            .AddLink(links[0])
            .SetRate(rate)
            .SetSsid("obss")
            .SetChannel(links[0].band, channels[links[0].band]);
        obss = obssBuilder.Build(obssApNode, obssStaNodes);
        PlaceOnCircle(obss, 5.0, Vector(20, 0, 0));
        InstallInternet(obss, 2);
    }
    PopulateArpCaches();

    TxAccounting txAccounting;
    txAccounting.Enable(bss.AllDevices());
    FlowSet flows;
    const Time start = MilliSeconds(100);
    const Time mStart = start + MilliSeconds(300);
    flows.SetMeasurementWindow(mStart, mStart + simTime);
    txAccounting.SetWindow(mStart, mStart + simTime);
    double sumPhy = 0;
    for (const auto& link : devLinks)
    {
        sumPhy += GetStandardDataRateMbps(mcs, link.width, 800, 1);
    }
    flows.AddUdpFlow("dl",
                     apNode.Get(0),
                     staNodes.Get(0),
                     bss.staIfs.GetAddress(0),
                     saturated ? sumPhy * 1.2 : loadMbps,
                     packetSize,
                     start,
                     mStart + simTime + MilliSeconds(1),
                     0,
                     saturated ? TrafficPattern::CBR : TrafficPattern::POISSON);
    FlowSet obssFlows;
    if (!saturated)
    {
        obssFlows.SetMeasurementWindow(mStart, mStart + simTime);
        const double obssPhy = GetStandardDataRateMbps(mcs, links[0].width, 800, 1);
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
    const auto stats = flows.GetStats(0);
    if (verbose)
    {
        std::cout << "  " << mode << (saturated ? " saturated: " : " low-load: ")
                  << txAccounting.Summary() << std::endl;
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
    double loadMbps{10};
    uint32_t packetSize{500};
    uint32_t payload{1472};
    Time simTime{Seconds(2)};
    EmlsrConfig emlsr;
    uint16_t auxPhyWidth{20};

    CommandLine cmd(__FILE__);
    AddCommonArgs(cmd, common);
    cmd.AddValue("links", "Two band:width links", linksStr);
    cmd.AddValue("mcs", "EHT MCS index", mcs);
    cmd.AddValue("loadMbps", "Offered load in the low-load scenario", loadMbps);
    cmd.AddValue("packetSize", "Packet size in the low-load scenario", packetSize);
    cmd.AddValue("payloadSize", "Packet size in the saturated scenario", payload);
    cmd.AddValue("simulationTime", "Measurement duration of each simulation", simTime);
    cmd.AddValue("emlsrPaddingDelay", "EMLSR padding delay", emlsr.paddingDelay);
    cmd.AddValue("emlsrTransitionDelay", "EMLSR transition delay", emlsr.transitionDelay);
    cmd.AddValue("emlsrAuxChWidth", "Channel width of the auxiliary PHY (MHz)", auxPhyWidth);
    cmd.AddValue("emlsrAuxSwitch", "Aux PHY switches to the main PHY link", emlsr.switchAuxPhy);
    cmd.AddValue("emlsrAuxTxCapable",
                 "Aux PHYs are capable of transmitting",
                 emlsr.auxPhyTxCapable);
    cmd.AddValue("emlsrManager", "EMLSR manager TypeId", emlsr.manager);
    cmd.Parse(argc, argv);
    ApplyCommonArgs(common);
    emlsr.auxPhyChannelWidth = auxPhyWidth;
    emlsr.linkSet = "0,1";
    emlsr.mainPhyId = 0;

    const auto links = ParseLinks(linksStr);
    NS_ABORT_MSG_IF(links.size() != 2, "Exactly two links are expected");
    if (common.quick)
    {
        simTime = std::min(simTime, MilliSeconds(300));
    }

    PrintBanner("EMLSR (enhanced multi-link single radio): 802.11be (Wi-Fi 7)",
                "Single-link vs dual-radio MLD vs EMLSR client: saturated DL throughput and "
                "low-load DL latency with contention on link 0.",
                {"ieee80211be", "chen2022", "galati2024", "ns3-wifi"});

    ResultTable table("wifi7-emlsr", common.outputDir);
    const double sldBound = [&]() {
        const auto [k, ppduUs] = SaturatedAmpdu(GetMpduBytes(payload),
                                                EHT_MAX_BA_WINDOW,
                                                EHT_MAX_AMPDU_BYTES,
                                                MAX_PPDU_DURATION_US,
                                                WIFI_STANDARD_80211be,
                                                mcs,
                                                links[0].width,
                                                800,
                                                1,
                                                links[0].band);
        MacModelTiming timing;
        timing.aifsUs = SIFS_US + GetStandardEdcaParams(AC_BE).aifsn * SLOT_US;
        timing.dataPpduUs = ppduUs;
        timing.ackPpduUs =
            ControlPpduDurationUs(GetBlockAckBytes(EHT_MAX_BA_WINDOW),
                                  ControlModeName(WIFI_STANDARD_80211be, links[0].band, mcs),
                                  links[0].band);
        return ComputeSingleUserGoodputMbps(timing, k, payload);
    }();

    FlowStats sldSat;
    FlowStats sldLow;
    for (const std::string mode : {"sld", "mld", "emlsr"})
    {
        for (const bool saturated : {true, false})
        {
            const auto stats = Run(mode,
                                   links,
                                   mcs,
                                   saturated,
                                   loadMbps,
                                   saturated ? payload : packetSize,
                                   emlsr,
                                   simTime,
                                   common.verbose);
            if (mode == "sld")
            {
                (saturated ? sldSat : sldLow) = stats;
            }
            ResultRow row;
            row.Set("mode", mode);
            row.Set("scenario", saturated ? "saturated-dl" : "low-load-dl-obss-on-link0");
            row.Set("mcs", mcs);
            row.Set("padding_delay_us",
                    static_cast<double>(emlsr.paddingDelay.GetMicroSeconds()),
                    0);
            row.Set("transition_delay_us",
                    static_cast<double>(emlsr.transitionDelay.GetMicroSeconds()),
                    0);
            row.Set("aux_phy_width_mhz", auxPhyWidth);
            row.Set("throughput_mbps", stats.throughputMbps, 2);
            row.Set("loss_ratio", stats.lossRatio, 4);
            row.Set("latency_p50_ms", stats.p50LatencyMs, 3);
            row.Set("latency_p95_ms", stats.p95LatencyMs, 3);
            row.Set("latency_p99_ms", stats.p99LatencyMs, 3);
            if (saturated)
            {
                row.Set("sld_bound_mbps", sldBound, 2);
                row.Set("measured", stats.throughputMbps, 2);
                if (mode == "sld")
                {
                    Compare(row,
                            stats.throughputMbps,
                            sldBound,
                            0.06,
                            "ieee80211be",
                            CheckKind::WITHIN);
                }
                else
                {
                    Compare(row,
                            stats.throughputMbps,
                            sldSat.throughputMbps,
                            0.0,
                            "chen2022",
                            CheckKind::INFO);
                }
            }
            else
            {
                row.Set("p95_gain_vs_sld",
                        stats.p95LatencyMs > 0 ? sldLow.p95LatencyMs / stats.p95LatencyMs : 0,
                        3);
                row.Set("measured", stats.p95LatencyMs, 3);
                Compare(row,
                        stats.p95LatencyMs,
                        sldLow.p95LatencyMs,
                        0.0,
                        "galati2024",
                        CheckKind::INFO);
            }
            table.AddRow(row);
        }
    }

    table.Print();
    table.Write();
    const auto fails = table.PrintSummary();
    return fails ? 1 : 0;
}
