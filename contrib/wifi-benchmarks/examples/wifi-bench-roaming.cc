/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file
 * @ingroup wifi-benchmarks
 *
 * BSS transition (roaming) packet loss and service interruption: the mobility baseline of the
 * 802.11bn (Wi-Fi 8) objective "at least 25% lower MPDU loss due to mobility".
 *
 * A station moves at constant speed from AP 1 to AP 2 (same SSID and channel, bridged to a wired
 * server) while exchanging bidirectional UDP traffic with the server. ns-3 stations do not roam
 * proactively: with the default behavior the station re-associates only after missing beacons
 * (MaxMissedBeacons), which is compared with a forced disassociation during the trip (an ideal
 * trigger, by default at 60% of the trip where AP 2 is the strongest candidate). The loss ratio and
 * the maximum gap between received packets (service interruption) are reported for both.
 * 802.11r/k/v fast transition mechanisms and the seamless roaming of 802.11bn are not modeled by
 * ns-3.
 *
 * Example: ./ns3 run "wifi-bench-roaming --speed=6 --apDistance=60"
 */

#include "ns3/boolean.h"
#include "ns3/bridge-helper.h"
#include "ns3/command-line.h"
#include "ns3/constant-velocity-mobility-model.h"
#include "ns3/csma-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/simulator.h"
#include "ns3/sta-wifi-mac.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"
#include "ns3/wifi-benchmark-helper.h"
#include "ns3/wifi-literature-reference.h"

#include <map>

using namespace ns3;
using namespace ns3::wifibench;

/// Outcome of one run
struct RunResult
{
    FlowStats ul;                                              ///< uplink flow
    FlowStats dl;                                              ///< downlink flow
    std::vector<std::pair<double, Mac48Address>> assocTimes;   ///< association times (s)
    std::vector<std::pair<double, Mac48Address>> deassocTimes; ///< disassociation times (s)
};

/**
 * Run one configuration.
 * @param forced whether a disassociation is forced during the trip
 * @param forcedFraction fraction of the trip at which the disassociation is forced
 * @param apDistance the AP-AP distance
 * @param speed the STA speed
 * @param exponent the path loss exponent
 * @param maxMissedBeacons the number of missed beacons that triggers a disassociation
 * @param loadMbps the offered load of each direction
 * @param packetSize the packet size
 * @param verbose print details
 * @return the run result
 */
RunResult
Run(bool forced,
    double forcedFraction,
    double apDistance,
    double speed,
    double exponent,
    uint32_t maxMissedBeacons,
    double loadMbps,
    uint32_t packetSize,
    bool verbose)
{
    NodeContainer staNode;
    staNode.Create(1);
    NodeContainer apNodes;
    apNodes.Create(2);
    NodeContainer serverNode;
    serverNode.Create(1);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("1Gbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MicroSeconds(1)));
    NodeContainer csmaNodes(apNodes, serverNode);
    NetDeviceContainer csmaDevices = csma.Install(csmaNodes);

    ChannelConfig chCfg;
    chCfg.logDistanceExponent = exponent;
    auto channel = CreateSpectrumChannel(Band::GHZ_5, chCfg);
    SpectrumWifiPhyHelper phy;
    phy.SetChannel(channel);
    phy.Set("ChannelSettings", StringValue("{36, 20, BAND_5GHZ, 0}"));
    phy.Set("TxPowerStart", DoubleValue(20));
    phy.Set("TxPowerEnd", DoubleValue(20));
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ax);
    wifi.SetRemoteStationManager("ns3::IdealWifiManager");
    Ssid ssid("roaming");
    WifiMacHelper mac;
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevices = wifi.Install(phy, mac, apNodes);
    mac.SetType("ns3::StaWifiMac",
                "Ssid",
                SsidValue(ssid),
                "ActiveProbing",
                BooleanValue(false),
                "MaxMissedBeacons",
                UintegerValue(maxMissedBeacons));
    NetDeviceContainer staDevice = wifi.Install(phy, mac, staNode);

    BridgeHelper bridge;
    for (uint32_t i = 0; i < 2; ++i)
    {
        bridge.Install(apNodes.Get(i), NetDeviceContainer(apDevices.Get(i), csmaDevices.Get(i)));
    }

    MobilityHelper mobility;
    auto positions = CreateObject<ListPositionAllocator>();
    positions->Add(Vector(0, 0, 0));
    positions->Add(Vector(apDistance, 0, 0));
    positions->Add(Vector(apDistance / 2, 50, 0));
    mobility.SetPositionAllocator(positions);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNodes);
    mobility.Install(serverNode);
    MobilityHelper staMobility;
    auto staPositions = CreateObject<ListPositionAllocator>();
    staPositions->Add(Vector(0, 5, 0));
    staMobility.SetPositionAllocator(staPositions);
    staMobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    staMobility.Install(staNode);

    InternetStackHelper stack;
    stack.Install(staNode);
    stack.Install(serverNode);
    Ipv4AddressHelper address;
    address.SetBase("10.1.0.0", "255.255.0.0");
    auto staIf = address.Assign(staDevice);
    auto serverIf = address.Assign(NetDeviceContainer(csmaDevices.Get(2)));

    const Time moveStart = Seconds(2);
    const Time travel = Seconds(apDistance / speed);
    const Time end = moveStart + travel + Seconds(2);
    Simulator::Schedule(moveStart, [&staNode, speed]() {
        staNode.Get(0)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(
            Vector(speed, 0, 0));
    });
    Simulator::Schedule(moveStart + travel, [&staNode]() {
        staNode.Get(0)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(0, 0, 0));
    });
    auto staMac = DynamicCast<StaWifiMac>(DynamicCast<WifiNetDevice>(staDevice.Get(0))->GetMac());
    if (forced)
    {
        // trigger slightly past the midpoint, where AP 2 is the strongest candidate
        Simulator::Schedule(moveStart + travel * forcedFraction,
                            [staMac]() { staMac->ForceDisassociation(true); });
    }
    RunResult res;
    std::vector<double> beaconTimes;
    if (verbose)
    {
        staMac->TraceConnectWithoutContext("BeaconArrival",
                                           MakeCallback(+[](std::vector<double>* v, Time t) {
                                               v->push_back(t.GetSeconds());
                                           }).Bind(&beaconTimes));
    }
    using EventList = std::vector<std::pair<double, Mac48Address>>;
    staMac->TraceConnectWithoutContext("Assoc", MakeCallback(+[](EventList* v, Mac48Address bssid) {
                                                    v->emplace_back(Simulator::Now().GetSeconds(),
                                                                    bssid);
                                                }).Bind(&res.assocTimes));
    staMac->TraceConnectWithoutContext("DeAssoc",
                                       MakeCallback(+[](EventList* v, Mac48Address bssid) {
                                           v->emplace_back(Simulator::Now().GetSeconds(), bssid);
                                       }).Bind(&res.deassocTimes));

    FlowSet flows;
    const Time start = Seconds(1);
    flows.SetMeasurementWindow(moveStart, end);
    const auto ulId = flows.AddUdpFlow("ul",
                                       staNode.Get(0),
                                       serverNode.Get(0),
                                       serverIf.GetAddress(0),
                                       loadMbps,
                                       packetSize,
                                       start,
                                       end);
    const auto dlId = flows.AddUdpFlow("dl",
                                       serverNode.Get(0),
                                       staNode.Get(0),
                                       staIf.GetAddress(0),
                                       loadMbps,
                                       packetSize,
                                       start,
                                       end);
    Simulator::Stop(end);
    Simulator::Run();
    res.ul = flows.GetStats(ulId);
    res.dl = flows.GetStats(dlId);
    if (verbose)
    {
        std::cout << "  assoc:";
        for (const auto& [t, bssid] : res.assocTimes)
        {
            std::cout << " " << t << "s->" << bssid;
        }
        std::cout << "; deassoc:";
        for (const auto& [t, bssid] : res.deassocTimes)
        {
            std::cout << " " << t << "s->" << bssid;
        }
        std::cout << std::endl;
        std::map<int, int> perSecond;
        for (auto t : beaconTimes)
        {
            perSecond[static_cast<int>(t)]++;
        }
        std::cout << "  beacons received per second:";
        for (const auto& [s, n] : perSecond)
        {
            std::cout << " " << s << "s:" << n;
        }
        std::cout << std::endl;
    }
    Simulator::Destroy();
    return res;
}

int
main(int argc, char* argv[])
{
    CommonArgs common;
    double apDistance{60};
    double speed{6};
    double exponent{3.5};
    uint32_t maxMissedBeacons{10};
    double loadMbps{2};
    uint32_t packetSize{500};
    double forcedFraction{0.6};

    CommandLine cmd(__FILE__);
    AddCommonArgs(cmd, common);
    cmd.AddValue("apDistance", "Distance between the two APs (m)", apDistance);
    cmd.AddValue("forcedFraction",
                 "Fraction of the trip at which the disassociation is forced (forced trigger)",
                 forcedFraction);
    cmd.AddValue("speed", "Speed of the station (m/s)", speed);
    cmd.AddValue("exponent", "Path loss exponent (log-distance model)", exponent);
    cmd.AddValue("maxMissedBeacons",
                 "Missed beacons before the station disassociates",
                 maxMissedBeacons);
    cmd.AddValue("loadMbps", "UDP load in each direction (Mb/s)", loadMbps);
    cmd.AddValue("packetSize", "Packet size in bytes", packetSize);
    cmd.Parse(argc, argv);
    ApplyCommonArgs(common);
    if (common.quick)
    {
        speed = std::max(speed, 12.0);
    }

    PrintBanner("BSS transition (roaming) loss and interruption baseline (802.11bn objective)",
                "Station moving between two bridged APs: loss ratio and maximum reception gap with "
                "missed-beacon-triggered vs forced (ideal) re-association.",
                {"ieee80211bn-par", "galati2024", "ns3-wifi"});

    ResultTable table("wifi8-roaming", common.outputDir);
    RunResult natural;
    for (const bool forced : {false, true})
    {
        const auto r = Run(forced,
                           forcedFraction,
                           apDistance,
                           speed,
                           exponent,
                           maxMissedBeacons,
                           loadMbps,
                           packetSize,
                           common.verbose);
        if (!forced)
        {
            natural = r;
        }
        ResultRow row;
        row.Set("trigger",
                forced
                    ? "forced-at-" + std::to_string(static_cast<int>(forcedFraction * 100)) + "pct"
                    : "missed-beacons");
        row.Set("ap_distance_m", apDistance, 1);
        row.Set("speed_mps", speed, 1);
        row.Set("max_missed_beacons", maxMissedBeacons);
        row.Set("n_associations", static_cast<uint32_t>(r.assocTimes.size()));
        row.Set("n_disassociations", static_cast<uint32_t>(r.deassocTimes.size()));
        row.Set("ul_loss_ratio", r.ul.lossRatio, 4);
        row.Set("dl_loss_ratio", r.dl.lossRatio, 4);
        row.Set("ul_max_gap_ms", r.ul.maxRxGapMs, 1);
        row.Set("dl_max_gap_ms", r.dl.maxRxGapMs, 1);
        row.Set("ul_throughput_mbps", r.ul.throughputMbps, 3);
        row.Set("dl_throughput_mbps", r.dl.throughputMbps, 3);
        const double loss = std::max(r.ul.lossRatio, r.dl.lossRatio);
        const double naturalLoss = std::max(natural.ul.lossRatio, natural.dl.lossRatio);
        row.Set("uhr_loss_target", naturalLoss * (1 - UHR_MOBILITY_LOSS_REDUCTION), 4);
        row.Set("meets_uhr_mobility_objective",
                loss <= naturalLoss * (1 - UHR_MOBILITY_LOSS_REDUCTION) ? "yes" : "no");
        row.Set("measured", loss, 4);
        Compare(row, loss, naturalLoss, 0.0, "ieee80211bn-par", CheckKind::INFO);
        table.AddRow(row);
    }

    table.Print();
    table.Write();
    const auto fails = table.PrintSummary();
    return fails ? 1 : 0;
}
