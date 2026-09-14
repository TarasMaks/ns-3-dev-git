/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file
 * @ingroup wifi-benchmarks
 *
 * EDCA channel access under saturation vs the Bianchi analytical model (Bianchi, IEEE JSAC 2000),
 * for HE (802.11ax) or EHT (802.11be) stations.
 *
 * N stations transmit saturated UDP uplink traffic to the AP, without RTS/CTS. The aggregate
 * throughput is compared with the Bianchi model extended with A-MPDU aggregation, as in the ns-3
 * reference script (src/wifi/examples/reference/bianchi11ax.py), fed with the PPDU durations of
 * the standard and the EDCA parameters of AC_BE (AIFSN 3, CWmin 15, CWmax 1023). Two variants of
 * the model are reported: DIFS after a collision and EIFS after a collision (ns-3 behavior).
 * The explicit BlockAckReq that ns-3 sends by default after a missed BlockAck is disabled so
 * that a collision costs one data PPDU, as assumed by the model.
 *
 * Example: ./ns3 run "wifi-bench-edca-bianchi --mcs=7 --nStations=5,10,20"
 */

#include "ns3/attribute-container.h"
#include "ns3/boolean.h"
#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"
#include "ns3/wifi-benchmark-helper.h"
#include "ns3/wifi-literature-reference.h"

#include <algorithm>
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

int
main(int argc, char* argv[])
{
    CommonArgs common;
    std::string standardStr{"ax"};
    uint32_t mcs{7};
    double channelWidthMhz{20};
    uint32_t payload{1472};
    std::string nStationsStr{"5,10,20,30,50"};
    std::string kListStr{"1,8,64"};
    Time simTime{Seconds(3)};
    double tolerance{0.08};

    CommandLine cmd(__FILE__);
    AddCommonArgs(cmd, common);
    cmd.AddValue("standard", "ax (Wi-Fi 6) or be (Wi-Fi 7)", standardStr);
    cmd.AddValue("mcs", "MCS index", mcs);
    cmd.AddValue("channelWidth", "Channel width in MHz", channelWidthMhz);
    cmd.AddValue("payloadSize", "UDP payload size in bytes", payload);
    cmd.AddValue("nStations", "Comma separated list of numbers of stations", nStationsStr);
    cmd.AddValue("mpdusPerAmpdu",
                 "Comma separated list of A-MPDU sizes in MPDUs (1 = no aggregation)",
                 kListStr);
    cmd.AddValue("simulationTime", "Measurement duration of each simulation", simTime);
    cmd.AddValue("tolerance", "Relative tolerance vs the Bianchi model (EIFS variant)", tolerance);
    cmd.Parse(argc, argv);
    ApplyCommonArgs(common);

    const WifiStandard standard = ParseStandard(standardStr);
    const bool eht = (standard == WIFI_STANDARD_80211be);
    const MHz_u width{channelWidthMhz};
    const Band band = Band::GHZ_5;
    const uint16_t gi = 800;
    const std::string name = eht ? "wifi7-edca-bianchi" : "wifi6-edca-bianchi";
    auto nList = ParseList(nStationsStr);
    auto kList = ParseList(kListStr);
    if (common.quick)
    {
        nList = {5, 10};
        kList = {1, 8};
        simTime = std::min(simTime, MilliSeconds(500));
    }

    PrintBanner("EDCA saturation throughput vs the Bianchi model: " + StandardName(standard),
                "N saturated uplink stations (no RTS/CTS); aggregate throughput vs Bianchi's "
                "model with A-MPDU aggregation (ns-3 reference script bianchi11ax.py).",
                {"bianchi2000", "ns3-wifi", "ieee80211-2020"});

    ResultTable table(name, common.outputDir);
    const std::string ctrlMode = ControlModeName(standard, band, mcs);
    // After a missed BlockAck, retransmit the A-MPDU instead of sending an explicit BlockAckReq,
    // which is the behavior assumed by the Bianchi model (a collision costs one data PPDU)
    Config::SetDefault("ns3::QosTxop::UseExplicitBarAfterMissedBlockAck", BooleanValue(false));
    const double phyRate = GetStandardDataRateMbps(mcs, width, gi, 1);

    for (const auto kRequested : kList)
    {
        uint32_t k = kRequested;
        const uint32_t mpduBytes = GetMpduBytes(payload);
        MacModelTiming timing;
        const auto edca = GetStandardEdcaParams(AC_BE);
        timing.aifsUs = SIFS_US + edca.aifsn * SLOT_US;
        timing.cwMin = edca.cwMin;
        timing.cwMax = edca.cwMax;
        timing.propagationDelayUs = 1.0 / 300; // 1 m
        uint32_t maxAmpdu = 0;
        uint16_t baWindow = 64;
        if (k == 1)
        {
            timing.dataPpduUs = PpduDurationUs(mpduBytes, standard, mcs, width, gi, 1, band);
            timing.ackPpduUs = ControlPpduDurationUs(14, ctrlMode, band);
        }
        else
        {
            baWindow = k;
            maxAmpdu = eht ? EHT_MAX_AMPDU_BYTES : HE_MAX_AMPDU_BYTES;
            const auto [kk, ppduUs] = SaturatedAmpdu(mpduBytes,
                                                     baWindow,
                                                     maxAmpdu,
                                                     MAX_PPDU_DURATION_US,
                                                     standard,
                                                     mcs,
                                                     width,
                                                     gi,
                                                     1,
                                                     band);
            if (kk != k)
            {
                std::cout << "A-MPDU of " << k << " MPDUs exceeds the max PPDU duration at MCS "
                          << mcs << ": using " << kk << " MPDUs" << std::endl;
                k = kk;
                baWindow = k;
            }
            timing.dataPpduUs = ppduUs;
            timing.ackPpduUs = ControlPpduDurationUs(GetBlockAckBytes(baWindow), ctrlMode, band);
        }

        for (const auto n : nList)
        {
            const double modelDifs = ComputeBianchiThroughputMbps(n, timing, k, payload, false);
            const double modelEifs = ComputeBianchiThroughputMbps(n, timing, k, payload, true);

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
            mac.guardIntervalNs = gi;
            mac.maxAmpduSize = maxAmpdu;
            mac.mpduBufferSize = baWindow;
            if (k == 1)
            {
                mac.baTids = {};
            }
            builder.SetStandard(standard).AddLink(link).SetRate(rate).SetMac(mac).SetChannel(
                band,
                CreateSpectrumChannel(band));
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
            for (uint32_t i = 0; i < n; ++i)
            {
                flows.AddUdpFlow("ul" + std::to_string(i),
                                 staNodes.Get(i),
                                 apNode.Get(0),
                                 bss.apIf.GetAddress(0),
                                 phyRate * 1.2 / n * 2,
                                 payload,
                                 start,
                                 mStart + simTime + MilliSeconds(1));
            }
            Simulator::Stop(mStart + simTime + MilliSeconds(1));
            Simulator::Run();
            const auto agg = flows.GetAggregateStats();
            if (common.verbose)
            {
                std::cout << "  n=" << n << " k=" << k << ": " << txAccounting.Summary()
                          << std::endl;
            }
            Simulator::Destroy();

            ResultRow row;
            row.Set("n_stations", n);
            row.Set("mpdus_per_ampdu", k);
            row.Set("mcs", mcs);
            row.Set("width_mhz", channelWidthMhz, 0);
            row.Set("phy_rate_mbps", phyRate, 1);
            row.Set("data_ppdu_us", timing.dataPpduUs, 1);
            row.Set("ack_ppdu_us", timing.ackPpduUs, 1);
            row.Set("bianchi_difs_mbps", modelDifs, 3);
            row.Set("bianchi_eifs_mbps", modelEifs, 3);
            row.Set("sim_data_ppdus", txAccounting.GetNPpdus(eht ? "EHT_MU" : "HE_SU"));
            row.Set("sim_ctrl_ppdus", txAccounting.GetNPpdus("LONG"));
            row.Set("measured", agg.throughputMbps, 3);
            Compare(row,
                    agg.throughputMbps,
                    modelEifs,
                    tolerance,
                    "bianchi2000",
                    CheckKind::WITHIN);
            table.AddRow(row);
        }
    }

    table.Print();
    table.Write();
    const auto fails = table.PrintSummary();
    return fails ? 1 : 0;
}
