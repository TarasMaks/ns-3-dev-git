/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file
 * @ingroup wifi-benchmarks
 *
 * Dynamic bandwidth operation under partial-band interference, and the gap to preamble
 * puncturing (802.11be).
 *
 * An 80 MHz BSS is interfered by a waveform generator occupying one secondary sub-channel with a
 * configurable duty cycle. ns-3 implements dynamic bandwidth operation (the transmitter falls back
 * to the widest channel whose secondaries are idle), while the MAC does not exploit the preamble
 * puncturing of 802.11be (see the wifi module documentation). The measured goodput is compared
 * with the analytical bounds of full bandwidth, ideal dynamic bandwidth and ideal puncturing
 * (data subcarriers of the punctured 20 MHz sub-channels removed), which quantifies the gain
 * expected from puncturing (Deng et al. 2020, Lopez-Perez et al. 2019).
 *
 * Example: ./ns3 run "wifi-bench-dynamic-bw --interferedSubchannel=secondary20 --dutyCycles=0,0.5"
 */

#include "ns3/command-line.h"
#include "ns3/double.h"
#include "ns3/mobility-helper.h"
#include "ns3/non-communicating-net-device.h"
#include "ns3/simulator.h"
#include "ns3/waveform-generator-helper.h"
#include "ns3/waveform-generator.h"
#include "ns3/wifi-benchmark-helper.h"
#include "ns3/wifi-literature-reference.h"
#include "ns3/wifi-spectrum-value-helper.h"
#include "ns3/wifi-utils.h"

#include <map>
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
    FlowStats flow;                       ///< flow statistics
    std::map<int, uint64_t> ppdusByWidth; ///< number of data PPDUs per channel width
};

/**
 * Run one scenario.
 * @param standard the standard
 * @param mcs the MCS
 * @param dutyCycle the interferer duty cycle (0 = no interferer)
 * @param period the interferer period
 * @param interfererCenterMhz the interferer center frequency
 * @param interfererWidth the interferer width
 * @param interfererPowerDbm the interferer power
 * @param payload the payload size
 * @param simTime the measurement duration
 * @param verbose print details
 * @return the run result
 */
RunResult
Run(WifiStandard standard,
    uint8_t mcs,
    double dutyCycle,
    Time period,
    double interfererCenterMhz,
    MHz_u interfererWidth,
    double interfererPowerDbm,
    uint32_t payload,
    Time simTime,
    bool verbose)
{
    const Band band = Band::GHZ_5;
    auto channel = CreateSpectrumChannel(band);
    NodeContainer apNode;
    apNode.Create(1);
    NodeContainer staNodes;
    staNodes.Create(1);
    BssBuilder builder;
    LinkConfig link;
    link.band = band;
    link.width = MHz_u{80};
    link.channelNumber = 42;
    RateConfig rate;
    rate.mcs = mcs;
    builder.SetStandard(standard).AddLink(link).SetRate(rate).SetChannel(band, channel);
    Bss bss = builder.Build(apNode, staNodes);
    InstallInternet(bss, 1);
    PopulateArpCaches();

    NodeContainer interfererNode;
    interfererNode.Create(1);
    MobilityHelper mobility;
    auto positions = CreateObject<ListPositionAllocator>();
    positions->Add(Vector(0, 0, 0));
    positions->Add(Vector(5, 0, 0));
    positions->Add(Vector(0, 5, 0));
    mobility.SetPositionAllocator(positions);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);
    mobility.Install(staNodes);
    mobility.Install(interfererNode);

    const Time start = MilliSeconds(100);
    const Time mStart = start + MilliSeconds(200);
    if (dutyCycle > 0)
    {
        auto psd = WifiSpectrumValueHelper::CreateHeOfdmTxPowerSpectralDensity(
            MHz_u{interfererCenterMhz},
            interfererWidth,
            DbmToW(dBm_u{interfererPowerDbm}),
            MHz_u{20});
        WaveformGeneratorHelper wgHelper;
        wgHelper.SetChannel(channel);
        wgHelper.SetTxPowerSpectralDensity(psd);
        wgHelper.SetPhyAttribute("Period", TimeValue(period));
        wgHelper.SetPhyAttribute("DutyCycle", DoubleValue(dutyCycle));
        auto wgDevices = wgHelper.Install(interfererNode);
        Simulator::Schedule(start,
                            &WaveformGenerator::Start,
                            wgDevices.Get(0)
                                ->GetObject<NonCommunicatingNetDevice>()
                                ->GetPhy()
                                ->GetObject<WaveformGenerator>());
    }

    TxAccounting txAccounting;
    txAccounting.Enable(bss.apDevice);
    FlowSet flows;
    flows.SetMeasurementWindow(mStart, mStart + simTime);
    txAccounting.SetWindow(mStart, mStart + simTime);
    const double phyRate = GetStandardDataRateMbps(mcs, MHz_u{80}, 800, 1);
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
    RunResult res;
    res.flow = flows.GetStats(0);
    for (const auto& r : txAccounting.GetRecords())
    {
        if (r.nMpdus > 1 || r.psduBytes > 200)
        {
            res.ppdusByWidth[static_cast<int>(r.widthMhz)]++;
        }
    }
    if (verbose)
    {
        std::cout << "  duty " << dutyCycle << ": " << txAccounting.Summary() << std::endl;
    }
    Simulator::Destroy();
    return res;
}

int
main(int argc, char* argv[])
{
    CommonArgs common;
    std::string standardStr{"be"};
    uint32_t mcs{7};
    std::string dutyCyclesStr{"0,0.25,0.5,0.75,1"};
    std::string subchannel{"secondary40"};
    Time period{MilliSeconds(10)};
    double interfererPowerDbm{10};
    uint32_t payload{1472};
    Time simTime{Seconds(2)};

    CommandLine cmd(__FILE__);
    AddCommonArgs(cmd, common);
    cmd.AddValue("standard", "ax (Wi-Fi 6) or be (Wi-Fi 7)", standardStr);
    cmd.AddValue("mcs", "MCS index", mcs);
    cmd.AddValue("dutyCycles", "Comma separated list of interferer duty cycles", dutyCyclesStr);
    cmd.AddValue("interferedSubchannel",
                 "Sub-channel of the 80 MHz channel 42 occupied by the interferer: secondary20 "
                 "(5190-5210 MHz) or secondary40 (5210-5250 MHz)",
                 subchannel);
    cmd.AddValue("interfererPeriod", "Period of the interferer on/off cycle", period);
    cmd.AddValue("interfererPower", "Interferer transmit power (dBm)", interfererPowerDbm);
    cmd.AddValue("payloadSize", "UDP payload size in bytes", payload);
    cmd.AddValue("simulationTime", "Measurement duration of each simulation", simTime);
    cmd.Parse(argc, argv);
    ApplyCommonArgs(common);

    const WifiStandard standard = ParseStandard(standardStr);
    const bool eht = (standard == WIFI_STANDARD_80211be);
    const std::string name = eht ? "wifi7-dynamic-bw" : "wifi6-dynamic-bw";
    auto dutyCycles = Split(dutyCyclesStr);
    if (common.quick)
    {
        dutyCycles = {"0", "0.5"};
        simTime = std::min(simTime, MilliSeconds(300));
    }
    double centerMhz;
    MHz_u interfererWidth;
    MHz_u fallbackWidth;
    uint16_t puncturedSubcarriers;
    if (subchannel == "secondary20")
    {
        centerMhz = 5200;
        interfererWidth = MHz_u{20};
        fallbackWidth = MHz_u{20};
        puncturedSubcarriers = 242;
    }
    else if (subchannel == "secondary40")
    {
        centerMhz = 5230;
        interfererWidth = MHz_u{40};
        fallbackWidth = MHz_u{40};
        puncturedSubcarriers = 484;
    }
    else
    {
        NS_ABORT_MSG("Unknown sub-channel " << subchannel);
    }

    PrintBanner(
        "Dynamic bandwidth operation vs preamble puncturing: " + StandardName(standard),
        "80 MHz BSS with a partial-band interferer of variable duty cycle: measured goodput "
        "vs the full-band, ideal dynamic-bandwidth and ideal puncturing bounds.",
        {"ieee80211be", "deng2020", "lopezperez2019", "ns3-wifi"});

    ResultTable table(name, common.outputDir);
    auto bound = [&](MHz_u width) {
        const auto [k, ppduUs] = SaturatedAmpdu(GetMpduBytes(payload),
                                                eht ? EHT_MAX_BA_WINDOW : HE_MAX_BA_WINDOW,
                                                eht ? EHT_MAX_AMPDU_BYTES : HE_MAX_AMPDU_BYTES,
                                                MAX_PPDU_DURATION_US,
                                                standard,
                                                mcs,
                                                width,
                                                800,
                                                1,
                                                Band::GHZ_5);
        MacModelTiming timing;
        timing.aifsUs = SIFS_US + GetStandardEdcaParams(AC_BE).aifsn * SLOT_US;
        timing.dataPpduUs = ppduUs;
        timing.ackPpduUs =
            ControlPpduDurationUs(GetBlockAckBytes(eht ? EHT_MAX_BA_WINDOW : HE_MAX_BA_WINDOW),
                                  ControlModeName(standard, Band::GHZ_5, mcs),
                                  Band::GHZ_5);
        return ComputeSingleUserGoodputMbps(timing, k, payload);
    };
    const double g80 = bound(MHz_u{80});
    const double gFallback = bound(fallbackWidth);
    // ideal puncturing: the MAC efficiency of 80 MHz with the rate scaled by the remaining
    // data subcarriers
    const double gPunctured = g80 * (980.0 - puncturedSubcarriers) / 980.0;

    for (const auto& dStr : dutyCycles)
    {
        const double duty = std::stod(dStr);
        const auto r = Run(standard,
                           mcs,
                           duty,
                           period,
                           centerMhz,
                           interfererWidth,
                           interfererPowerDbm,
                           payload,
                           simTime,
                           common.verbose);
        const double dynBound = (1 - duty) * g80 + duty * gFallback;
        const double punctBound = (1 - duty) * g80 + duty * gPunctured;
        ResultRow row;
        row.Set("interfered_subchannel", subchannel);
        row.Set("duty_cycle", duty, 2);
        row.Set("mcs", mcs);
        row.Set("full_band_bound_mbps", g80, 2);
        row.Set("dynamic_bw_bound_mbps", dynBound, 2);
        row.Set("puncturing_bound_mbps", punctBound, 2);
        row.Set("puncturing_gain_vs_dynamic", dynBound > 0 ? punctBound / dynBound : 0, 3);
        for (const int w : {20, 40, 80})
        {
            row.Set("ppdus_" + std::to_string(w) + "mhz",
                    static_cast<uint64_t>(r.ppdusByWidth.count(w) ? r.ppdusByWidth.at(w) : 0));
        }
        row.Set("loss_ratio", r.flow.lossRatio, 4);
        row.Set("measured", r.flow.throughputMbps, 2);
        Compare(row,
                r.flow.throughputMbps,
                dynBound,
                0.0,
                "ns3-wifi",
                duty == 0 ? CheckKind::WITHIN : CheckKind::INFO);
        if (duty == 0)
        {
            // no interference: the full-band bound applies within the usual tolerance
            Compare(row, r.flow.throughputMbps, g80, 0.06, "ieee80211be", CheckKind::WITHIN);
        }
        table.AddRow(row);
    }

    table.Print();
    table.Write();
    const auto fails = table.PrintSummary();
    return fails ? 1 : 0;
}
