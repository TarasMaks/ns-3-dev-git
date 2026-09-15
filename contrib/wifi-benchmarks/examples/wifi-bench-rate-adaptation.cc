/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file
 * @ingroup wifi-benchmarks
 *
 * Goodput vs distance with rate adaptation (Ideal, MinstrelHt, ThompsonSampling) for 802.11ax or
 * 802.11be, compared with a "genie" bound: for each distance the SNR is computed from the
 * log-distance path loss model and the bound is the maximum over the MCSs of the analytical
 * single-user goodput multiplied by the MPDU success probability of the table-based error model
 * (the model used by the simulation), and zero beyond the range at which the preamble can be
 * detected on the primary 20 MHz channel. The Ideal manager, which knows the SNR, selects the highest
 * MCS whose SNR threshold for the configured BER is met: its goodput is checked against the
 * goodput expected from that rule, while the genie bound and the statistical managers are
 * reported for information (a 1 s warm-up lets them converge).
 *
 * Example: ./ns3 run "wifi-bench-rate-adaptation --distances=5,20,60 --channelWidth=80"
 */

#include "ns3/command-line.h"
#include "ns3/double.h"
#include "ns3/simulator.h"
#include "ns3/table-based-error-rate-model.h"
#include "ns3/wifi-benchmark-helper.h"
#include "ns3/wifi-literature-reference.h"
#include "ns3/wifi-tx-vector.h"

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

int
main(int argc, char* argv[])
{
    CommonArgs common;
    std::string standardStr{"ax"};
    double channelWidthMhz{80};
    std::string distancesStr{"1,5,10,20,30,40,50,60,80,100"};
    std::string managersStr{
        "ns3::IdealWifiManager,ns3::MinstrelHtWifiManager,ns3::ThompsonSamplingWifiManager"};
    uint32_t payload{1472};
    Time simTime{Seconds(2)};
    double exponent{3.0};
    double txPowerDbm{20};
    double tolerance{0.1};
    double berThreshold{1e-6};
    double preambleMinRssiDbm{-82};

    CommandLine cmd(__FILE__);
    AddCommonArgs(cmd, common);
    cmd.AddValue("standard", "ax (Wi-Fi 6) or be (Wi-Fi 7)", standardStr);
    cmd.AddValue("channelWidth", "Channel width in MHz", channelWidthMhz);
    cmd.AddValue("distances", "Comma separated list of AP-STA distances in meters", distancesStr);
    cmd.AddValue("managers", "Comma separated list of rate manager TypeIds", managersStr);
    cmd.AddValue("payloadSize", "UDP payload size in bytes", payload);
    cmd.AddValue("simulationTime", "Measurement duration of each simulation", simTime);
    cmd.AddValue("exponent", "Path loss exponent of the log-distance model", exponent);
    cmd.AddValue("txPower", "Transmit power in dBm", txPowerDbm);
    cmd.AddValue("tolerance",
                 "Relative tolerance between the Ideal manager goodput and the goodput expected "
                 "from its MCS selection rule",
                 tolerance);
    cmd.AddValue("berThreshold", "BER threshold of the Ideal manager", berThreshold);
    cmd.AddValue("preambleMinRssi",
                 "Minimum RSSI (dBm, primary 20 MHz) for preamble detection",
                 preambleMinRssiDbm);
    cmd.Parse(argc, argv);
    ApplyCommonArgs(common);

    const WifiStandard standard = ParseStandard(standardStr);
    const bool eht = (standard == WIFI_STANDARD_80211be);
    const MHz_u width{channelWidthMhz};
    const Band band = (width > MHz_u{160}) ? Band::GHZ_6 : Band::GHZ_5;
    const uint16_t gi = 800;
    const double noiseFigureDb = 7;
    const std::string name = eht ? "wifi7-rate-adaptation" : "wifi6-rate-adaptation";
    auto distances = Split(distancesStr);
    auto managers = Split(managersStr);
    if (common.quick)
    {
        distances = {"5", "40"};
        managers = {"ns3::IdealWifiManager", "ns3::MinstrelHtWifiManager"};
        simTime = std::min(simTime, MilliSeconds(300));
    }

    PrintBanner("Goodput vs distance with rate adaptation: " + StandardName(standard),
                "Single STA, log-distance path loss; measured goodput of the rate managers vs a "
                "genie bound (best MCS given the SNR and the table-based error model).",
                {"patidar2017", "ns3-wifi", "ieee80211ax"});

    ResultTable table(name, common.outputDir);
    ChannelConfig chCfg;
    chCfg.logDistanceExponent = exponent;
    const double refLoss = 46.6777;
    if (band == Band::GHZ_6)
    {
        chCfg.referenceLossDb = 48.0;
    }
    const double refLossUsed = chCfg.referenceLossDb.value_or(refLoss);
    const double noiseDbm = GetNoisePowerDbm(width, noiseFigureDb);
    auto errorModel = CreateObject<TableBasedErrorRateModel>();
    const uint16_t baWindow = eht ? EHT_MAX_BA_WINDOW : HE_MAX_BA_WINDOW;
    const uint32_t maxAmpdu = eht ? EHT_MAX_AMPDU_BYTES : HE_MAX_AMPDU_BYTES;
    const uint32_t mpduBytes = GetMpduBytes(payload);

    for (const auto& dStr : distances)
    {
        const double d = std::stod(dStr);
        const double pathLossDb = refLossUsed + 10 * exponent * std::log10(std::max(d, 1.0));
        const double rxDbm = txPowerDbm - pathLossDb;
        const double snrDb = rxDbm - noiseDbm;
        const double snr = std::pow(10.0, snrDb / 10.0);
        // the preamble is detected on the primary 20 MHz channel: the RSSI measured there is
        // lower than the total RSSI by 10 log10(width / 20 MHz) and must exceed the detection
        // threshold (ThresholdPreambleDetectionModel::MinimumRssi, -82 dBm by default)
        const double primary20RssiDbm = rxDbm - 10 * std::log10(width / MHz_u{20});
        const bool detectable = primary20RssiDbm >= preambleMinRssiDbm;

        // genie bound and expected goodput of the Ideal manager (highest MCS whose SNR
        // threshold for the BER target is below the SNR)
        double genie = 0;
        int genieMcs = -1;
        double idealExpected = 0;
        int idealMcs = -1;
        for (uint8_t mcs = 0; mcs <= MaxMcs(standard); ++mcs)
        {
            WifiTxVector txVector;
            txVector.SetMode(WifiMode(DataModeName(standard, mcs)));
            txVector.SetChannelWidth(width);
            txVector.SetNss(1);
            txVector.SetGuardInterval(NanoSeconds(gi));
            txVector.SetLdpc(true);
            txVector.SetPreambleType(eht ? WIFI_PREAMBLE_EHT_MU : WIFI_PREAMBLE_HE_SU);
            const double psr =
                errorModel->GetChunkSuccessRate(txVector.GetMode(), txVector, snr, mpduBytes * 8);
            const auto [k, ppduUs] = SaturatedAmpdu(mpduBytes,
                                                    baWindow,
                                                    maxAmpdu,
                                                    MAX_PPDU_DURATION_US,
                                                    standard,
                                                    mcs,
                                                    width,
                                                    gi,
                                                    1,
                                                    band);
            MacModelTiming timing;
            timing.aifsUs = SIFS_US + GetStandardEdcaParams(AC_BE).aifsn * SLOT_US;
            timing.dataPpduUs = ppduUs;
            timing.ackPpduUs = ControlPpduDurationUs(GetBlockAckBytes(baWindow),
                                                     ControlModeName(standard, band, mcs),
                                                     band);
            const double g =
                detectable ? ComputeSingleUserGoodputMbps(timing, k, payload) * psr : 0.0;
            if (g > genie)
            {
                genie = g;
                genieMcs = mcs;
            }
            // the Ideal manager computes its thresholds without LDPC (BCC error tables)
            WifiTxVector thresholdVector = txVector;
            thresholdVector.SetLdpc(false);
            const double threshold = errorModel->CalculateSnr(thresholdVector, berThreshold);
            if (snr > threshold)
            {
                idealExpected = g;
                idealMcs = mcs;
            }
        }

        for (const auto& manager : managers)
        {
            const bool ideal = (manager == "ns3::IdealWifiManager");
            NodeContainer apNode;
            apNode.Create(1);
            NodeContainer staNodes;
            staNodes.Create(1);
            BssBuilder builder;
            LinkConfig link;
            link.band = band;
            link.width = width;
            PhyConfig phy;
            phy.txPower = dBm_u{txPowerDbm};
            phy.rxNoiseFigureDb = noiseFigureDb;
            RateConfig rate;
            rate.mcs = -1;
            rate.manager = manager;
            if (ideal)
            {
                builder.SetWifiCustomizer([berThreshold](WifiHelper& wifi) {
                    wifi.SetRemoteStationManager("ns3::IdealWifiManager",
                                                 "BerThreshold",
                                                 DoubleValue(berThreshold));
                });
            }
            MacConfig mac;
            mac.guardIntervalNs = gi;
            builder.SetStandard(standard)
                .AddLink(link)
                .SetPhy(phy)
                .SetRate(rate)
                .SetMac(mac)
                .SetChannel(band, CreateSpectrumChannel(band, chCfg));
            Bss bss = builder.Build(apNode, staNodes);
            PlaceOnCircle(bss, d);
            InstallInternet(bss, 1);
            PopulateArpCaches();

            TxAccounting txAccounting;
            txAccounting.Enable(bss.AllDevices());
            FlowSet flows;
            const Time start = MilliSeconds(100);
            const Time mStart = start + (common.quick ? MilliSeconds(300) : Seconds(1));
            flows.SetMeasurementWindow(mStart, mStart + simTime);
            txAccounting.SetWindow(mStart, mStart + simTime);
            const double phyRateMax = GetStandardDataRateMbps(MaxMcs(standard), width, gi, 1);
            flows.AddUdpFlow("dl",
                             apNode.Get(0),
                             staNodes.Get(0),
                             bss.staIfs.GetAddress(0),
                             phyRateMax * 1.2,
                             payload,
                             start,
                             mStart + simTime + MilliSeconds(1));
            Simulator::Stop(mStart + simTime + MilliSeconds(1));
            Simulator::Run();
            const auto stats = flows.GetStats(0);
            const std::string dominantMode = txAccounting.GetDominantMode(eht ? "EHT_MU" : "HE_SU");
            if (common.verbose)
            {
                std::cout << "  d=" << d << " " << manager << ": " << txAccounting.Summary()
                          << std::endl;
            }
            Simulator::Destroy();

            ResultRow row;
            row.Set("distance_m", d, 1);
            row.Set("rx_power_dbm", rxDbm, 1);
            row.Set("primary20_rssi_dbm", primary20RssiDbm, 1);
            row.Set("preamble_detectable", detectable ? "yes" : "no");
            row.Set("snr_db", snrDb, 1);
            row.Set("manager", manager.substr(5));
            row.Set("width_mhz", channelWidthMhz, 0);
            row.Set("genie_mcs", genieMcs);
            row.Set("genie_goodput_mbps", genie, 2);
            row.Set("ideal_rule_mcs", idealMcs);
            row.Set("ideal_rule_goodput_mbps", idealExpected, 2);
            row.Set("sim_dominant_mode", dominantMode);
            row.Set("ratio_to_genie", genie > 0 ? stats.throughputMbps / genie : 0, 3);
            row.Set("measured", stats.throughputMbps, 2);
            Compare(row,
                    stats.throughputMbps,
                    ideal ? idealExpected : genie,
                    tolerance,
                    "patidar2017",
                    ideal ? CheckKind::WITHIN : CheckKind::INFO);
            table.AddRow(row);
        }
    }

    table.Print();
    table.Write();
    const auto fails = table.PrintSummary();
    return fails ? 1 : 0;
}
