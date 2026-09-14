/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file
 * @ingroup wifi-benchmarks
 *
 * Spatial reuse with BSS coloring and OBSS-PD (802.11ax) between two overlapping BSSs.
 *
 * Two BSSs (AP1-STA1 and AP2-STA2, BSS colors 1 and 2) share the same channel; the STAs send
 * saturated uplink traffic. The AP-AP distance and the OBSS-PD threshold are swept. With the
 * Friis path loss the level of the OBSS signal received by a STA is computed and compared with
 * the threshold to predict whether spatial reuse (concurrent transmissions) is possible; when it
 * is and no transmit power reduction applies (OBSS-PD levels above TxPowerRef - TxPower + OBSS_PD_min
 * lower the transmit power of the spatial reuse transmissions), the aggregate throughput is
 * expected to approach twice the single-BSS throughput (Wilhelmi et al. 2021, Khorov et al.
 * 2019). The CCA busy fraction of STA1 is also reported.
 *
 * Example: ./ns3 run "wifi-bench-spatial-reuse --apDistances=100,150 --obssPdLevels=none,-72"
 */

#include "ns3/command-line.h"
#include "ns3/double.h"
#include "ns3/mobility-helper.h"
#include "ns3/simulator.h"
#include "ns3/wifi-benchmark-helper.h"
#include "ns3/wifi-literature-reference.h"
#include "ns3/wifi-phy.h"

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

/// Outcome of one run
struct RunResult
{
    double tput1{0};       ///< BSS 1 goodput
    double tput2{0};       ///< BSS 2 goodput
    double ccaBusySta1{0}; ///< CCA busy fraction of STA1
    double txSta1{0};      ///< TX fraction of STA1
};

/**
 * Run one scenario.
 * @param standard the standard
 * @param mcs the MCS
 * @param width the channel width
 * @param d1 the AP-STA distance
 * @param d3 the AP-AP distance
 * @param obssPd the OBSS-PD level (nullopt = disabled)
 * @param txPowerSta the STA TX power
 * @param txPowerAp the AP TX power
 * @param payload the payload size
 * @param uplink whether traffic is uplink
 * @param simTime the measurement duration
 * @return the run result
 */
RunResult
Run(WifiStandard standard,
    uint8_t mcs,
    MHz_u width,
    double d1,
    double d3,
    std::optional<double> obssPd,
    double txPowerSta,
    double txPowerAp,
    uint32_t payload,
    bool uplink,
    Time simTime)
{
    const Band band = Band::GHZ_5;
    ChannelConfig chCfg;
    chCfg.lossModel = "friis";
    auto channel = CreateSpectrumChannel(band, chCfg);

    std::vector<Bss> bsss;
    for (uint8_t i = 0; i < 2; ++i)
    {
        NodeContainer apNode;
        apNode.Create(1);
        NodeContainer staNodes;
        staNodes.Create(1);
        BssBuilder builder;
        LinkConfig link;
        link.band = band;
        link.width = width;
        PhyConfig phy;
        phy.txPower = dBm_u{txPowerSta};
        RateConfig rate;
        rate.mcs = mcs;
        MacConfig mac;
        builder.SetStandard(standard)
            .AddLink(link)
            .SetPhy(phy)
            .SetRate(rate)
            .SetMac(mac)
            .SetSsid("bss-" + std::to_string(i + 1))
            .SetBssColor(i + 1)
            .SetChannel(band, channel);
        if (obssPd)
        {
            builder.SetObssPdLevel(dBm_u{*obssPd});
        }
        Bss bss = builder.Build(apNode, staNodes);
        auto apPhy = bss.ApDev()->GetPhy(0);
        apPhy->SetTxPowerStart(dBm_u{txPowerAp});
        apPhy->SetTxPowerEnd(dBm_u{txPowerAp});

        MobilityHelper mobility;
        auto positions = CreateObject<ListPositionAllocator>();
        const double x = i * d3;
        positions->Add(Vector(x, 0, 0));
        positions->Add(Vector(x, d1, 0));
        mobility.SetPositionAllocator(positions);
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(apNode);
        mobility.Install(staNodes);
        InstallInternet(bss, i + 1);
        bsss.push_back(bss);
    }
    PopulateArpCaches();

    WifiCoTraceHelper coTrace;
    coTrace.Enable(bsss[0].staDevices);
    FlowSet flows;
    const Time start = MilliSeconds(100);
    const Time mStart = start + MilliSeconds(300);
    flows.SetMeasurementWindow(mStart, mStart + simTime);
    coTrace.Start(mStart);
    coTrace.Stop(mStart + simTime);
    const double phyRate = GetStandardDataRateMbps(mcs, width, 800, 1);
    for (uint8_t i = 0; i < 2; ++i)
    {
        auto& bss = bsss[i];
        if (uplink)
        {
            flows.AddUdpFlow("ul-bss" + std::to_string(i + 1),
                             bss.staNodes.Get(0),
                             bss.apNode.Get(0),
                             bss.apIf.GetAddress(0),
                             phyRate * 1.2,
                             payload,
                             start,
                             mStart + simTime + MilliSeconds(1));
        }
        else
        {
            flows.AddUdpFlow("dl-bss" + std::to_string(i + 1),
                             bss.apNode.Get(0),
                             bss.staNodes.Get(0),
                             bss.staIfs.GetAddress(0),
                             phyRate * 1.2,
                             payload,
                             start,
                             mStart + simTime + MilliSeconds(1));
        }
    }
    Simulator::Stop(mStart + simTime + MilliSeconds(1));
    Simulator::Run();
    RunResult res;
    res.tput1 = flows.GetStats(0).throughputMbps;
    res.tput2 = flows.GetStats(1).throughputMbps;
    const auto sta1 = bsss[0].staNodes.Get(0);
    res.ccaBusySta1 = StateFraction(coTrace,
                                    sta1->GetId(),
                                    bsss[0].StaDev(0)->GetIfIndex(),
                                    0,
                                    {WifiPhyState::CCA_BUSY});
    res.txSta1 = StateFraction(coTrace,
                               sta1->GetId(),
                               bsss[0].StaDev(0)->GetIfIndex(),
                               0,
                               {WifiPhyState::TX});
    Simulator::Destroy();
    return res;
}

int
main(int argc, char* argv[])
{
    CommonArgs common;
    std::string standardStr{"ax"};
    uint32_t mcs{3};
    double channelWidthMhz{20};
    double d1{30};
    std::string apDistancesStr{"100,150,300"};
    std::string levelsStr{"none,-82,-77,-72,-67,-62"};
    double txPowerSta{10};
    double txPowerAp{21};
    double txPowerRef{21};
    uint32_t payload{1472};
    bool uplink{true};
    Time simTime{Seconds(2)};

    CommandLine cmd(__FILE__);
    AddCommonArgs(cmd, common);
    cmd.AddValue("standard", "ax (Wi-Fi 6) or be (Wi-Fi 7)", standardStr);
    cmd.AddValue("mcs", "MCS index", mcs);
    cmd.AddValue("channelWidth", "Channel width in MHz", channelWidthMhz);
    cmd.AddValue("apStaDistance", "Distance between each AP and its STA (m)", d1);
    cmd.AddValue("apDistances", "Comma separated list of AP-AP distances (m)", apDistancesStr);
    cmd.AddValue("obssPdLevels",
                 "Comma separated list of OBSS-PD levels in dBm (none = disabled)",
                 levelsStr);
    cmd.AddValue("txPowerSta", "STA transmit power (dBm)", txPowerSta);
    cmd.AddValue("txPowerAp", "AP transmit power (dBm)", txPowerAp);
    cmd.AddValue("txPowerRef",
                 "OBSS-PD reference transmit power (dBm) used by the power reduction rule",
                 txPowerRef);
    cmd.AddValue("payloadSize", "UDP payload size in bytes", payload);
    cmd.AddValue("uplink", "Uplink traffic if true, downlink otherwise", uplink);
    cmd.AddValue("simulationTime", "Measurement duration of each simulation", simTime);
    cmd.Parse(argc, argv);
    ApplyCommonArgs(common);

    const WifiStandard standard = ParseStandard(standardStr);
    const bool eht = (standard == WIFI_STANDARD_80211be);
    const MHz_u width{channelWidthMhz};
    const std::string name = eht ? "wifi7-spatial-reuse" : "wifi6-spatial-reuse";
    auto distances = Split(apDistancesStr);
    auto levels = Split(levelsStr);
    if (common.quick)
    {
        distances = {"150"};
        levels = {"none", "-72"};
        simTime = std::min(simTime, MilliSeconds(300));
    }

    PrintBanner("Spatial reuse (BSS color / OBSS-PD): " + StandardName(standard),
                "Two overlapping BSSs with saturated traffic; aggregate throughput vs the OBSS-PD "
                "threshold and the OBSS signal level (concurrent transmissions expected when the "
                "OBSS level is below the threshold).",
                {"ieee80211ax", "wilhelmi2021", "khorov2019", "ns3-wifi"});

    ResultTable table(name, common.outputDir);
    const double phyRate = GetStandardDataRateMbps(mcs, width, 800, 1);
    const double freqHz = 5.5e9;
    for (const auto& dStr : distances)
    {
        const double d3 = std::stod(dStr);
        // OBSS signal level at a STA: the other STA (uplink) or AP (downlink) at distance d3
        const double txObss = uplink ? txPowerSta : txPowerAp;
        const double friisDb = 20 * std::log10(4 * M_PI * d3 * freqHz / 3e8);
        const double rxObss = txObss - friisDb;
        double baseline = 0;
        for (const auto& lStr : levels)
        {
            std::optional<double> level;
            if (lStr != "none")
            {
                level = std::stod(lStr);
            }
            RunResult r = Run(standard,
                              mcs,
                              width,
                              d1,
                              d3,
                              level,
                              txPowerSta,
                              txPowerAp,
                              payload,
                              uplink,
                              simTime);
            const double aggregate = r.tput1 + r.tput2;
            if (!level)
            {
                baseline = aggregate;
            }
            const bool detectable = rxObss >= -82; // preamble detection floor
            // OBSS-PD rule of the standard: a transmission made after ignoring an OBSS PPDU is
            // limited to TxPowerRef - (level - OBSS_PD_min); levels that reduce the power of
            // the data transmitter trade spatial reuse for a lower SINR, so the throughput gain
            // is only checked when no reduction applies (Wilhelmi et al. 2021)
            const double txData = uplink ? txPowerSta : txPowerAp;
            const double txLimit = level ? txPowerRef - (*level - (-82.0)) : txData;
            const double txReduction = std::max(0.0, txData - txLimit);
            const bool srExpected = level && detectable && (rxObss < *level) && (txReduction <= 0);
            ResultRow row;
            row.Set("ap_distance_m", d3, 0);
            row.Set("obss_pd_level_dbm", lStr);
            row.Set("mcs", mcs);
            row.Set("width_mhz", channelWidthMhz, 0);
            row.Set("phy_rate_mbps", phyRate, 1);
            row.Set("rx_obss_dbm", rxObss, 1);
            row.Set("obss_detectable", detectable ? "yes" : "no");
            row.Set("tx_power_reduction_db", txReduction, 1);
            row.Set("sr_expected", srExpected ? "yes" : "no");
            row.Set("tput_bss1_mbps", r.tput1, 2);
            row.Set("tput_bss2_mbps", r.tput2, 2);
            row.Set("cca_busy_frac_sta1", r.ccaBusySta1, 3);
            row.Set("tx_frac_sta1", r.txSta1, 3);
            row.Set("gain_vs_no_sr", baseline > 0 ? aggregate / baseline : 0, 3);
            row.Set("measured", aggregate, 2);
            if (srExpected)
            {
                // concurrent transmissions: aggregate expected to approach 2x the baseline
                Compare(row, aggregate, 2 * baseline, 0.3, "wilhelmi2021", CheckKind::AT_LEAST);
            }
            else
            {
                Compare(row, aggregate, baseline, 0.0, "wilhelmi2021", CheckKind::INFO);
            }
            table.AddRow(row);
        }
    }

    table.Print();
    table.Write();
    const auto fails = table.PrintSummary();
    return fails ? 1 : 0;
}
