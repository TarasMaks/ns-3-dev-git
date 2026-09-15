/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file
 * @ingroup wifi-benchmarks
 *
 * Power saving of a station with sporadic downlink traffic: legacy power save mode (PS-Poll,
 * buffered at the AP until the next beacon/DTIM) vs active mode, measured with the
 * WifiRadioEnergyModel. Target Wake Time (TWT), the 802.11ax scheduled power save mechanism, is
 * not modeled by ns-3 (see the module documentation), so the legacy mechanism is used as the
 * available proxy; the energy is compared with the theoretical minimum ratio between the sleep
 * and idle currents, and the latency with the beacon interval (buffered frames are delivered
 * after the next beacon). Nurchis and Bellalta (2019) discuss the energy/latency trade-off of
 * TWT compared to legacy power save.
 *
 * Example: ./ns3 run "wifi-bench-power-save --packetInterval=50ms"
 */

#include "ns3/boolean.h"
#include "ns3/command-line.h"
#include "ns3/simulator.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"
#include "ns3/wifi-benchmark-helper.h"
#include "ns3/wifi-literature-reference.h"
#include "ns3/wifi-mac.h"

using namespace ns3;
using namespace ns3::wifibench;

/// Outcome of one run
struct RunResult
{
    FlowStats flow;    ///< flow statistics
    double energyJ{0}; ///< energy consumed by the STA in the window
};

/**
 * Run one scenario.
 * @param standard the standard
 * @param width the channel width
 * @param powerSave whether the STA uses power save mode
 * @param packetSize the packet size
 * @param packetInterval the packet interval
 * @param beaconInterval the beacon interval
 * @param simTime the measurement duration
 * @param idleA idle current
 * @param sleepA sleep current
 * @return the run result
 */
RunResult
Run(WifiStandard standard,
    MHz_u width,
    bool powerSave,
    uint32_t packetSize,
    Time packetInterval,
    Time beaconInterval,
    Time simTime,
    double idleA,
    double sleepA)
{
    const Band band = Band::GHZ_5;
    NodeContainer apNode;
    apNode.Create(1);
    NodeContainer staNodes;
    staNodes.Create(1);
    BssBuilder builder;
    LinkConfig link;
    link.band = band;
    link.width = width;
    RateConfig rate;
    rate.mcs = 7;
    MacConfig mac;
    mac.staticSetup = false; // beacons are needed by power save
    builder.SetStandard(standard)
        .AddLink(link)
        .SetRate(rate)
        .SetMac(mac)
        .SetChannel(band, CreateSpectrumChannel(band))
        .SetApMacCustomizer([beaconInterval](WifiMacHelper& apMac) {
            apMac.SetType("ns3::ApWifiMac", "BeaconInterval", TimeValue(beaconInterval));
        })
        .SetStaMacCustomizer([powerSave](WifiMacHelper& staMac) {
            // "<link id> <enable>" pairs
            staMac.SetPowerSaveManager("ns3::DefaultPowerSaveManager",
                                       "PowerSaveMode",
                                       StringValue(powerSave ? "0 true" : ""));
        });
    Bss bss = builder.Build(apNode, staNodes);
    PlaceOnCircle(bss, 5.0);
    InstallInternet(bss, 1);
    auto energy = InstallEnergy(bss.staDevices, idleA, 0.380, 0.313, sleepA, idleA);

    FlowSet flows;
    const Time start = Seconds(1);
    const Time mStart = Seconds(2);
    flows.SetMeasurementWindow(mStart, mStart + simTime);
    const double rateMbps = packetSize * 8.0 / packetInterval.GetSeconds() / 1e6;
    flows.AddUdpFlow("dl",
                     apNode.Get(0),
                     staNodes.Get(0),
                     bss.staIfs.GetAddress(0),
                     rateMbps,
                     packetSize,
                     start,
                     mStart + simTime + MilliSeconds(1));
    double energyAtStart = 0;
    Simulator::Schedule(mStart, [&]() { energyAtStart = energy.GetConsumedJoules(0); });
    Simulator::Stop(mStart + simTime + MilliSeconds(1));
    Simulator::Run();
    RunResult res;
    res.flow = flows.GetStats(0);
    res.energyJ = energy.GetConsumedJoules(0) - energyAtStart;
    Simulator::Destroy();
    return res;
}

int
main(int argc, char* argv[])
{
    CommonArgs common;
    std::string standardStr{"ax"};
    double channelWidthMhz{20};
    uint32_t packetSize{200};
    Time packetInterval{MilliSeconds(100)};
    Time beaconInterval{MicroSeconds(102400)};
    Time simTime{Seconds(5)};
    double idleA{0.273};
    double sleepA{0.033};
    double voltage{3.0};

    CommandLine cmd(__FILE__);
    AddCommonArgs(cmd, common);
    cmd.AddValue("standard", "ax (Wi-Fi 6) or be (Wi-Fi 7)", standardStr);
    cmd.AddValue("channelWidth", "Channel width in MHz", channelWidthMhz);
    cmd.AddValue("packetSize", "Downlink packet size in bytes", packetSize);
    cmd.AddValue("packetInterval", "Downlink packet interval", packetInterval);
    cmd.AddValue("beaconInterval", "Beacon interval", beaconInterval);
    cmd.AddValue("simulationTime", "Measurement duration of each simulation", simTime);
    cmd.AddValue("idleCurrent", "Idle current of the radio (A)", idleA);
    cmd.AddValue("sleepCurrent", "Sleep current of the radio (A)", sleepA);
    cmd.Parse(argc, argv);
    ApplyCommonArgs(common);

    const WifiStandard standard = ParseStandard(standardStr);
    const bool eht = (standard == WIFI_STANDARD_80211be);
    const MHz_u width{channelWidthMhz};
    const std::string name = eht ? "wifi7-power-save" : "wifi6-power-save";
    if (common.quick)
    {
        simTime = std::min(simTime, Seconds(1));
    }

    PrintBanner("Power save mode energy and latency: " + StandardName(standard),
                "Sporadic downlink traffic to a STA in active vs legacy power save mode; energy vs "
                "the theoretical sleep/idle ratio, latency vs the beacon interval (TWT is not "
                "modeled by ns-3).",
                {"nurchis2019", "ieee80211ax", "ns3-wifi"});

    ResultTable table(name, common.outputDir);
    const double idlePowerW = idleA * voltage;
    RunResult active = Run(standard,
                           width,
                           false,
                           packetSize,
                           packetInterval,
                           beaconInterval,
                           simTime,
                           idleA,
                           sleepA);
    RunResult ps = Run(standard,
                       width,
                       true,
                       packetSize,
                       packetInterval,
                       beaconInterval,
                       simTime,
                       idleA,
                       sleepA);
    const double minRatio = sleepA / idleA;
    for (const bool isPs : {false, true})
    {
        const RunResult& r = isPs ? ps : active;
        ResultRow row;
        row.Set("mode", isPs ? "power-save" : "active");
        row.Set("packet_interval_ms", packetInterval.GetMilliSeconds(), 1);
        row.Set("beacon_interval_ms", beaconInterval.GetMicroSeconds() / 1000.0, 1);
        row.Set("rx_packets", r.flow.rxPackets);
        row.Set("loss_ratio", r.flow.lossRatio, 4);
        row.Set("latency_mean_ms", r.flow.meanLatencyMs, 3);
        row.Set("latency_p95_ms", r.flow.p95LatencyMs, 3);
        row.Set("latency_max_ms", r.flow.maxLatencyMs, 3);
        row.Set("energy_j", r.energyJ, 4);
        row.Set("avg_power_w", r.energyJ / simTime.GetSeconds(), 4);
        row.Set("energy_ratio_vs_active", active.energyJ > 0 ? r.energyJ / active.energyJ : 0, 4);
        row.Set("min_energy_ratio_sleep_idle", minRatio, 4);
        row.Set("measured", r.energyJ / simTime.GetSeconds(), 4);
        if (isPs)
        {
            // power save must consume less than idle listening
            Compare(row,
                    r.energyJ / simTime.GetSeconds(),
                    idlePowerW,
                    0.0,
                    "nurchis2019",
                    CheckKind::AT_MOST);
        }
        else
        {
            // active mode: average power close to the idle power (traffic is sporadic)
            Compare(row,
                    r.energyJ / simTime.GetSeconds(),
                    idlePowerW,
                    0.1,
                    "ns3-wifi",
                    CheckKind::WITHIN);
        }
        table.AddRow(row);
    }
    ResultRow lat;
    lat.Set("mode", "power-save-latency");
    lat.Set("packet_interval_ms", packetInterval.GetMilliSeconds(), 1);
    lat.Set("beacon_interval_ms", beaconInterval.GetMicroSeconds() / 1000.0, 1);
    lat.Set("latency_mean_ms", ps.flow.meanLatencyMs, 3);
    lat.Set("latency_p95_ms", ps.flow.p95LatencyMs, 3);
    lat.Set("measured", ps.flow.p95LatencyMs, 3);
    // buffered frames are delivered after the next beacon: P95 latency within ~1 beacon interval
    Compare(lat,
            ps.flow.p95LatencyMs,
            beaconInterval.GetMicroSeconds() / 1000.0,
            0.5,
            "ieee80211-2020",
            CheckKind::AT_MOST);
    table.AddRow(lat);

    table.Print();
    table.Write();
    const auto fails = table.PrintSummary();
    return fails ? 1 : 0;
}
