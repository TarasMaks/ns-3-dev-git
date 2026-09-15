/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file
 * @ingroup wifi-benchmarks
 *
 * Frame aggregation (A-MPDU, A-MSDU) and Block Ack window benchmark for 802.11ax/802.11be.
 *
 * A saturated UDP downlink between an AP and one STA is simulated for several aggregation
 * settings: no aggregation, 64 KB A-MPDU (802.11n/ac era limit), the 64-MPDU Block Ack window,
 * the 256-MPDU extended Block Ack window of 802.11ax (up to 6.5 MB A-MPDUs), the 1024-MPDU
 * window of 802.11be (up to 15.5 MB A-MPDUs), and A-MSDU aggregation. The goodput is compared
 * with an analytical single-user bound built from the PPDU durations of the standard.
 *
 * Example: ./ns3 run "wifi-bench-aggregation --standard=ax --mcs=11 --channelWidth=80"
 */

#include "ns3/command-line.h"
#include "ns3/simulator.h"
#include "ns3/wifi-benchmark-helper.h"
#include "ns3/wifi-literature-reference.h"

#include <cmath>

using namespace ns3;
using namespace ns3::wifibench;

/// One aggregation configuration
struct AggConfig
{
    std::string label;     ///< label
    uint32_t maxAmpdu;     ///< max A-MPDU size (0 = disabled)
    uint32_t maxAmsdu;     ///< max A-MSDU size (0 = disabled)
    uint16_t baWindow;     ///< Block Ack window
    bool blockAck;         ///< whether a Block Ack agreement is set up
    std::string reference; ///< citation
};

int
main(int argc, char* argv[])
{
    CommonArgs common;
    std::string standardStr{"ax"};
    int mcs{-1};
    double channelWidthMhz{80};
    uint32_t payload{1472};
    Time simTime{Seconds(1)};
    double tolerance{0.06};

    CommandLine cmd(__FILE__);
    AddCommonArgs(cmd, common);
    cmd.AddValue("standard", "ax (Wi-Fi 6) or be (Wi-Fi 7)", standardStr);
    cmd.AddValue("mcs", "MCS index (-1 = highest of the standard)", mcs);
    cmd.AddValue("channelWidth", "Channel width in MHz", channelWidthMhz);
    cmd.AddValue("payloadSize", "UDP payload size in bytes", payload);
    cmd.AddValue("simulationTime", "Measurement duration of each simulation", simTime);
    cmd.AddValue("tolerance", "Relative tolerance of the goodput checks", tolerance);
    cmd.Parse(argc, argv);
    ApplyCommonArgs(common);

    const WifiStandard standard = ParseStandard(standardStr);
    const bool eht = (standard == WIFI_STANDARD_80211be);
    if (mcs < 0)
    {
        mcs = MaxMcs(standard);
    }
    const MHz_u width{channelWidthMhz};
    const Band band = (width > MHz_u{160}) ? Band::GHZ_6 : Band::GHZ_5;
    const uint16_t gi = 800;
    const std::string name = eht ? "wifi7-aggregation" : "wifi6-aggregation";
    if (common.quick)
    {
        simTime = std::min(simTime, MilliSeconds(300));
    }

    PrintBanner("Frame aggregation and Block Ack window: " + StandardName(standard),
                "Saturated single-user goodput for A-MPDU/A-MSDU/Block Ack settings vs the "
                "analytical bound (max A-MPDU 6.5 MB and BA window 256 in 802.11ax, 15.5 MB and "
                "1024 in 802.11be).",
                {"ieee80211ax", "ieee80211be", "khorov2019", "deng2020"});

    std::vector<AggConfig> configs{
        {"no-aggregation", 0, 0, 64, false, "ieee80211-2020"},
        {"ampdu-64KB-ba64", 65535, 0, 64, true, "ieee80211-2020"},
        {"ampdu-max-ba64", HE_MAX_AMPDU_BYTES, 0, 64, true, "ieee80211ax"},
        {"ampdu-max-ba256", HE_MAX_AMPDU_BYTES, 0, 256, true, "ieee80211ax"},
        {"amsdu-8KB-ampdu-max-ba256", HE_MAX_AMPDU_BYTES, 7935, 256, true, "ieee80211ax"},
    };
    if (eht)
    {
        configs.push_back({"ampdu-max-ba1024", EHT_MAX_AMPDU_BYTES, 0, 1024, true, "ieee80211be"});
        configs.push_back(
            {"amsdu-8KB-ampdu-max-ba1024", EHT_MAX_AMPDU_BYTES, 7935, 1024, true, "ieee80211be"});
    }
    if (common.quick)
    {
        configs = {configs[0], configs[3]};
    }

    ResultTable table(name, common.outputDir);
    for (const auto& cfg : configs)
    {
        // analytical bound
        uint32_t msdusPerMpdu = 1;
        uint32_t mpduBytes = GetMpduBytes(payload);
        if (cfg.maxAmsdu > 0)
        {
            const uint32_t msdu = LLC_SNAP_BYTES + IPV4_HEADER_BYTES + UDP_HEADER_BYTES + payload;
            const uint32_t subframe = 14 + msdu;
            const uint32_t padded = ((subframe + 3) / 4) * 4;
            msdusPerMpdu = 0;
            while ((msdusPerMpdu + 1) * padded - (padded - subframe) <= cfg.maxAmsdu)
            {
                ++msdusPerMpdu;
            }
            msdusPerMpdu = std::max<uint32_t>(msdusPerMpdu, 1);
            mpduBytes = QOS_MAC_HEADER_BYTES + (msdusPerMpdu - 1) * padded + subframe + FCS_BYTES;
        }
        uint32_t k = 1;
        double ppduUs = 0;
        double ackUs = 0;
        const std::string ctrlMode = ControlModeName(standard, band, mcs);
        if (cfg.maxAmpdu == 0)
        {
            ppduUs = PpduDurationUs(mpduBytes, standard, mcs, width, gi, 1, band);
            ackUs = ControlPpduDurationUs(14, ctrlMode, band);
        }
        else
        {
            std::tie(k, ppduUs) = SaturatedAmpdu(mpduBytes,
                                                 cfg.baWindow,
                                                 cfg.maxAmpdu,
                                                 MAX_PPDU_DURATION_US,
                                                 standard,
                                                 mcs,
                                                 width,
                                                 gi,
                                                 1,
                                                 band);
            ackUs = ControlPpduDurationUs(GetBlockAckBytes(cfg.baWindow), ctrlMode, band);
        }
        MacModelTiming timing;
        timing.aifsUs = SIFS_US + GetStandardEdcaParams(AC_BE).aifsn * SLOT_US;
        timing.dataPpduUs = ppduUs;
        timing.ackPpduUs = ackUs;
        const double bound = ComputeSingleUserGoodputMbps(timing, k, payload * msdusPerMpdu);

        // simulation
        NodeContainer apNode;
        apNode.Create(1);
        NodeContainer staNodes;
        staNodes.Create(1);
        BssBuilder builder;
        LinkConfig link;
        link.band = band;
        link.width = width;
        RateConfig rate;
        rate.mcs = mcs;
        MacConfig mac;
        mac.guardIntervalNs = gi;
        mac.maxAmpduSize = cfg.maxAmpdu;
        mac.maxAmsduSize = cfg.maxAmsdu;
        mac.mpduBufferSize = cfg.baWindow;
        if (!cfg.blockAck)
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

        const double phyRate = GetStandardDataRateMbps(mcs, width, gi, 1);
        TxAccounting txAccounting;
        txAccounting.Enable(bss.AllDevices());
        FlowSet flows;
        const Time start = MilliSeconds(100);
        const Time mStart = start + MilliSeconds(100);
        flows.SetMeasurementWindow(mStart, mStart + simTime);
        txAccounting.SetWindow(mStart, mStart + simTime);
        flows.AddUdpFlow("dl",
                         apNode.Get(0),
                         staNodes.Get(0),
                         bss.staIfs.GetAddress(0),
                         phyRate * 1.2,
                         payload,
                         start,
                         mStart + simTime + MilliSeconds(1));
        Simulator::Stop(mStart + simTime + MilliSeconds(1));
        Simulator::Run();
        const double goodput = flows.GetStats(0).throughputMbps;
        if (common.verbose)
        {
            std::cout << "  " << cfg.label << ": " << txAccounting.Summary() << std::endl;
        }
        Simulator::Destroy();

        ResultRow row;
        row.Set("config", cfg.label);
        row.Set("mcs", mcs);
        row.Set("width_mhz", channelWidthMhz, 0);
        row.Set("max_ampdu_bytes", cfg.maxAmpdu);
        row.Set("max_amsdu_bytes", cfg.maxAmsdu);
        row.Set("ba_window", cfg.baWindow);
        row.Set("model_mpdus_per_ampdu", k);
        row.Set("model_msdus_per_mpdu", msdusPerMpdu);
        row.Set("model_ppdu_us", ppduUs, 1);
        row.Set("sim_mean_mpdus_per_ppdu", txAccounting.GetMeanMpdusPerPpdu(), 1);
        row.Set("phy_rate_mbps", phyRate, 1);
        row.Set("mac_efficiency_pct", goodput / phyRate * 100, 1);
        row.Set("measured", goodput, 2);
        Compare(row, goodput, bound, tolerance, cfg.reference, CheckKind::WITHIN);
        table.AddRow(row);
    }

    table.Print();
    table.Write();
    const auto fails = table.PrintSummary();
    return fails ? 1 : 0;
}
