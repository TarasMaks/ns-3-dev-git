/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file
 * @ingroup wifi-benchmarks
 *
 * Uplink OFDMA (trigger-based access) vs EDCA contention for 802.11ax/802.11be.
 *
 * N saturated stations transmit to the AP. Four access modes are compared: EDCA contention,
 * UL OFDMA scheduled with Basic Trigger Frames (buffer status learned from the QoS Control field
 * of received frames), UL OFDMA with BSRP Trigger Frames (buffer status reports), and the latter
 * with the MU EDCA parameters advertised by the AP disabling EDCA access of the stations for a
 * given timer after a triggered transmission (pure trigger-based access). The EDCA throughput is
 * compared with the Bianchi model and the OFDMA throughput with the PHY bound (sum of the per-RU
 * rates of the standard); the literature reports that trigger-based access removes collisions so
 * that the throughput does not decrease with the number of stations (Khorov et al. 2019).
 *
 * Example: ./ns3 run "wifi-bench-ul-ofdma --nStations=4,8 --muEdcaTimer=8192us"
 */

#include "ns3/boolean.h"
#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/simulator.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"
#include "ns3/wifi-benchmark-helper.h"
#include "ns3/wifi-literature-reference.h"

#include <set>
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
    FlowStats aggregate;                ///< aggregate flow statistics
    double minStaMbps{0};               ///< minimum per-STA goodput
    double maxStaMbps{0};               ///< maximum per-STA goodput
    RuType ruType{RuType::RU_TYPE_MAX}; ///< dominant RU type of the TB PPDUs
    uint64_t tbPpdus{0};                ///< number of TB PPDUs (per STA)
    double meanStasPerTrigger{0};       ///< mean number of stations transmitting per trigger round
    uint64_t suPpdus{0};                ///< number of SU data PPDUs (contention)
    uint64_t triggers{0};               ///< number of trigger frames
    double meanMpdusPerTb{0};           ///< mean MPDUs per TB PPDU
};

/**
 * Run one scenario.
 * @param standard the standard
 * @param n the number of STAs
 * @param mcs the MCS
 * @param width the channel width
 * @param mode "edca", "ofdma", "ofdma-bsrp" or "ofdma-bsrp-muedca"
 * @param payload the payload size
 * @param simTime the measurement duration
 * @param muEdcaTimer the MU EDCA timer (0 = MU EDCA disabled)
 * @param accessReqInterval the scheduler access request interval
 * @param txopLimit the TXOP limit of the AP (OFDMA modes)
 * @param verbose print details
 * @return the run result
 */
RunResult
Run(WifiStandard standard,
    uint32_t n,
    uint8_t mcs,
    MHz_u width,
    const std::string& mode,
    uint32_t payload,
    Time simTime,
    Time muEdcaTimer,
    Time accessReqInterval,
    Time txopLimit,
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
    if (mode != "edca")
    {
        MuSchedulerConfig mu;
        mu.dlAckType = "AGGR-MU-BAR";
        mu.enableUlOfdma = true;
        mu.enableBsrp = (mode != "ofdma");
        mu.nStations = n;
        mu.accessReqInterval = accessReqInterval;
        // minimum PSDU size each solicited station must be able to send; the granted duration
        // is derived from the buffer status reports, capped by the max PPDU duration
        mu.ulPsduSize = GetMpduBytes(payload);
        builder.SetMuScheduler(mu);
        // a TXOP limit lets the AP send the Basic Trigger Frame after the BSRP exchange in the
        // same TXOP (with a null TXOP limit every TXOP would start with a new BSRP)
        builder.SetApMacCustomizer([txopLimit](WifiMacHelper& apMac) {
            apMac.SetEdca(AC_BE,
                          "TxopLimits",
                          StringValue(std::to_string(txopLimit.GetMicroSeconds()) + "us"));
        });
        if (mode == "ofdma-bsrp-muedca" && muEdcaTimer.IsStrictlyPositive())
        {
            // the MU EDCA Parameter Set is advertised only if the timers of all ACs are set
            builder.SetWifiCustomizer([muEdcaTimer](WifiHelper& wifi) {
                for (const std::string ac : {"Be", "Bk", "Vi", "Vo"})
                {
                    wifi.ConfigHeOptions("Mu" + ac + "Aifsn", UintegerValue(0));
                    wifi.ConfigHeOptions("Mu" + ac + "CwMin", UintegerValue(15));
                    wifi.ConfigHeOptions("Mu" + ac + "CwMax", UintegerValue(1023));
                    wifi.ConfigHeOptions(ac + "MuEdcaTimer", TimeValue(muEdcaTimer));
                }
            });
        }
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
        flows.AddUdpFlow("ul" + std::to_string(i),
                         staNodes.Get(i),
                         apNode.Get(0),
                         bss.apIf.GetAddress(0),
                         phyRate * 2.0 / n,
                         payload,
                         start,
                         mStart + simTime + MilliSeconds(1));
    }
    Simulator::Stop(mStart + simTime + MilliSeconds(1));
    Simulator::Run();

    RunResult res;
    res.aggregate = flows.GetAggregateStats();
    res.minStaMbps = flows.GetThroughputPercentile(0);
    res.maxStaMbps = flows.GetThroughputPercentile(100);
    const std::string tbPreamble = eht ? "EHT_TB" : "HE_TB";
    res.ruType = txAccounting.GetDominantRuType(tbPreamble);
    double tbMpdus = 0;
    std::set<Time> tbRounds;
    for (const auto& r : txAccounting.GetRecords())
    {
        if (r.preamble == tbPreamble)
        {
            res.tbPpdus++;
            tbMpdus += r.nMpdus;
            tbRounds.insert(r.time);
        }
        else if (r.preamble == "LONG" && r.nodeId == apNode.Get(0)->GetId() && r.psduBytes > 30)
        {
            // trigger frames (and multi-STA BlockAcks) are sent by the AP in non-HT format
            res.triggers++;
        }
        else if ((r.preamble == "HE_SU" || r.preamble == "EHT_MU") && r.nUsers == 1 &&
                 r.nodeId != apNode.Get(0)->GetId() && r.psduBytes > 100)
        {
            res.suPpdus++;
        }
    }
    res.meanMpdusPerTb = res.tbPpdus ? tbMpdus / res.tbPpdus : 0;
    res.meanStasPerTrigger =
        tbRounds.empty() ? 0 : static_cast<double>(res.tbPpdus) / tbRounds.size();
    if (verbose)
    {
        std::cout << "  n=" << n << " " << mode << ": " << txAccounting.Summary() << std::endl;
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
    std::string nStationsStr{"4,8"};
    std::string modesStr{"edca,ofdma,ofdma-bsrp,ofdma-bsrp-muedca"};
    uint32_t payload{1472};
    Time simTime{Seconds(2)};
    Time muEdcaTimer{MicroSeconds(8192)};
    Time accessReqInterval{MicroSeconds(100)};
    Time txopLimit{MicroSeconds(8160)};
    double bianchiTolerance{0.10};

    CommandLine cmd(__FILE__);
    AddCommonArgs(cmd, common);
    cmd.AddValue("standard", "ax (Wi-Fi 6) or be (Wi-Fi 7)", standardStr);
    cmd.AddValue("mcs", "MCS index", mcs);
    cmd.AddValue("channelWidth", "Channel width in MHz", channelWidthMhz);
    cmd.AddValue("nStations", "Comma separated list of numbers of stations", nStationsStr);
    cmd.AddValue("modes",
                 "Comma separated list among edca, ofdma, ofdma-bsrp, ofdma-bsrp-muedca",
                 modesStr);
    cmd.AddValue("payloadSize", "UDP payload size in bytes", payload);
    cmd.AddValue("simulationTime", "Measurement duration of each simulation", simTime);
    cmd.AddValue("muEdcaTimer",
                 "MU EDCA timer of the ofdma-bsrp-muedca mode (EDCA access of the stations is "
                 "suspended for this duration after a triggered transmission)",
                 muEdcaTimer);
    cmd.AddValue("accessReqInterval",
                 "Interval between the channel access requests of the MU scheduler",
                 accessReqInterval);
    cmd.AddValue("txopLimit", "TXOP limit of the AP in the OFDMA modes", txopLimit);
    cmd.AddValue("bianchiTolerance",
                 "Relative tolerance of the EDCA vs Bianchi check",
                 bianchiTolerance);
    cmd.Parse(argc, argv);
    ApplyCommonArgs(common);

    const WifiStandard standard = ParseStandard(standardStr);
    const bool eht = (standard == WIFI_STANDARD_80211be);
    const MHz_u width{channelWidthMhz};
    const Band band = (width > MHz_u{160}) ? Band::GHZ_6 : Band::GHZ_5;
    const std::string name = eht ? "wifi7-ul-ofdma" : "wifi6-ul-ofdma";
    auto nList = Split(nStationsStr);
    auto modes = Split(modesStr);
    if (common.quick)
    {
        nList = {"4"};
        modes = {"edca", "ofdma-bsrp", "ofdma-bsrp-muedca"};
        simTime = std::min(simTime, MilliSeconds(300));
    }
    // a collision costs one data PPDU in the Bianchi model
    Config::SetDefault("ns3::QosTxop::UseExplicitBarAfterMissedBlockAck", BooleanValue(false));

    PrintBanner("Uplink OFDMA (trigger-based) vs EDCA contention: " + StandardName(standard),
                "Saturated uplink of N stations: EDCA vs the Bianchi model, UL OFDMA vs the per-RU "
                "PHY bound of the standard.",
                {"ieee80211ax", "khorov2019", "bianchi2000"});

    ResultTable table(name, common.outputDir);
    const double phyRate = GetStandardDataRateMbps(mcs, width, 800, 1);
    const uint16_t baWindow = eht ? EHT_MAX_BA_WINDOW : HE_MAX_BA_WINDOW;
    const uint32_t mpduBytes = GetMpduBytes(payload);
    const auto [k, ppduUs] = SaturatedAmpdu(mpduBytes,
                                            baWindow,
                                            eht ? EHT_MAX_AMPDU_BYTES : HE_MAX_AMPDU_BYTES,
                                            MAX_PPDU_DURATION_US,
                                            standard,
                                            mcs,
                                            width,
                                            800,
                                            1,
                                            band);
    MacModelTiming timing;
    const auto edca = GetStandardEdcaParams(AC_BE);
    timing.aifsUs = SIFS_US + edca.aifsn * SLOT_US;
    timing.dataPpduUs = ppduUs;
    timing.ackPpduUs = ControlPpduDurationUs(GetBlockAckBytes(baWindow),
                                             ControlModeName(standard, band, mcs),
                                             band);

    for (const auto& nStr : nList)
    {
        const uint32_t n = std::stoul(nStr);
        const double bianchi = ComputeBianchiThroughputMbps(n, timing, k, payload, true);
        double edcaTput = 0;
        for (const auto& mode : modes)
        {
            RunResult r = Run(standard,
                              n,
                              mcs,
                              width,
                              mode,
                              payload,
                              simTime,
                              muEdcaTimer,
                              accessReqInterval,
                              txopLimit,
                              common.verbose);
            std::ostringstream ruStr;
            double ruRate = 0;
            if (r.ruType != RuType::RU_TYPE_MAX)
            {
                ruStr << r.ruType;
                ruRate = GetStandardRuDataRateMbps(mcs, r.ruType, 800, 1);
            }
            else
            {
                ruStr << "none";
            }
            // stations solicited per trigger round may be fewer than n (RU availability)
            const double bound = (mode == "edca") ? phyRate : ruRate * r.meanStasPerTrigger;
            if (mode == "edca")
            {
                edcaTput = r.aggregate.throughputMbps;
            }
            ResultRow row;
            row.Set("n_stations", n);
            row.Set("mode", mode);
            row.Set("mcs", mcs);
            row.Set("width_mhz", channelWidthMhz, 0);
            row.Set("mu_edca_timer_us",
                    mode == "ofdma-bsrp-muedca" ? static_cast<double>(muEdcaTimer.GetMicroSeconds())
                                                : 0.0,
                    0);
            row.Set("ru_type", mode == "edca" ? "full" : ruStr.str());
            row.Set("phy_bound_mbps", bound, 1);
            row.Set("bianchi_eifs_mbps", bianchi, 2);
            row.Set("tb_ppdus", r.tbPpdus);
            row.Set("stas_per_trigger", r.meanStasPerTrigger, 2);
            row.Set("mean_mpdus_per_tb", r.meanMpdusPerTb, 1);
            row.Set("contention_ppdus", r.suPpdus);
            row.Set("ap_ctrl_ppdus", r.triggers);
            row.Set("min_sta_mbps", r.minStaMbps, 2);
            row.Set("max_sta_mbps", r.maxStaMbps, 2);
            row.Set("gain_vs_edca", edcaTput > 0 ? r.aggregate.throughputMbps / edcaTput : 0, 3);
            row.Set("mac_efficiency_pct",
                    bound > 0 ? r.aggregate.throughputMbps / bound * 100 : 0,
                    1);
            row.Set("measured", r.aggregate.throughputMbps, 2);
            if (mode == "edca")
            {
                Compare(row,
                        r.aggregate.throughputMbps,
                        bianchi,
                        bianchiTolerance,
                        "bianchi2000",
                        CheckKind::WITHIN);
            }
            else
            {
                Compare(row,
                        r.aggregate.throughputMbps,
                        bound,
                        0.0,
                        "ieee80211ax",
                        CheckKind::AT_MOST);
            }
            table.AddRow(row);
        }
    }

    table.Print();
    table.Write();
    const auto fails = table.PrintSummary();
    return fails ? 1 : 0;
}
