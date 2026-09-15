/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file
 * @ingroup ble
 *
 * Coexistence of BLE and Wi-Fi in the 2.4 GHz band.
 *
 * Because the BLE model transmits over a spectrum channel, it shares the band with every other
 * ns-3 technology that does the same. This example puts a BLE connection and a saturated
 * 802.11n Wi-Fi link on the same spectrum channel and measures what each of them carries, on
 * its own and together.
 *
 * A 20 MHz Wi-Fi channel covers about ten of the two megahertz wide BLE channels, so a
 * connection hopping over the 37 data channels spends part of its events inside the Wi-Fi
 * signal. Disabling those channels in the channel map of the connection, which is what adaptive
 * frequency hopping does, recovers most of the loss. The example reports all three cases.
 *
 * Example: ./ns3 run "ble-wifi-coexistence --wifiChannel=6 --avoidWifi=true"
 */

#include "ns3/applications-module.h"
#include "ns3/ble-helper.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/wifi-module.h"

#include <cmath>
#include <iomanip>

using namespace ns3;

namespace
{

/// Octets the BLE peripheral received.
uint64_t g_bleBytes = 0;

/**
 * Count a delivered BLE payload.
 * @param packet the payload
 * @param from the address it came from
 */
void
OnBleReceive(Ptr<Packet> packet, Mac48Address from)
{
    g_bleBytes += packet->GetSize();
}

/**
 * The centre frequency of a 2.4 GHz Wi-Fi channel.
 * @param channel the channel number
 * @return the frequency in MHz
 */
double
WifiChannelFrequencyMhz(uint32_t channel)
{
    return 2407.0 + 5.0 * channel;
}

/**
 * Run one scenario.
 *
 * @param withWifi whether the Wi-Fi link transmits
 * @param avoidWifi whether the BLE connection removes the channels the Wi-Fi link occupies
 * @param wifiChannel the Wi-Fi channel number
 * @param duration how long the scenario is measured
 * @param bleKbps filled with the BLE throughput
 * @param wifiMbps filled with the Wi-Fi throughput
 * @param blockedChannels filled with the number of BLE channels removed from the map
 */
void
Run(bool withWifi,
    bool avoidWifi,
    uint32_t wifiChannel,
    Time duration,
    double& bleKbps,
    double& wifiMbps,
    uint32_t& blockedChannels)
{
    g_bleBytes = 0;
    blockedChannels = 0;

    // one spectrum channel carries both technologies
    auto spectrumChannel = CreateObject<MultiModelSpectrumChannel>();
    auto loss = CreateObject<LogDistancePropagationLossModel>();
    loss->SetAttribute("Exponent", DoubleValue(3.0));
    spectrumChannel->AddPropagationLossModel(loss);
    spectrumChannel->SetPropagationDelayModel(CreateObject<ConstantSpeedPropagationDelayModel>());

    NodeContainer bleNodes;
    bleNodes.Create(2);
    NodeContainer wifiNodes;
    wifiNodes.Create(2);

    MobilityHelper mobility;
    auto positions = CreateObject<ListPositionAllocator>();
    positions->Add(Vector(0.0, 0.0, 0.0)); // BLE central
    positions->Add(Vector(1.0, 0.0, 0.0)); // BLE peripheral
    positions->Add(Vector(3.0, 0.0, 0.0)); // Wi-Fi access point
    positions->Add(Vector(4.0, 0.0, 0.0)); // Wi-Fi station
    mobility.SetPositionAllocator(positions);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(bleNodes);
    mobility.Install(wifiNodes);

    // the BLE connection
    ble::BleHelper bleHelper;
    bleHelper.SetChannel(spectrumChannel);
    bleHelper.SetLinkLayerAttribute("QueueSize", UintegerValue(200000));
    auto bleDevices = bleHelper.Install(bleNodes);
    ble::BleHelper::AssignStreams(bleDevices, 1);
    auto central = DynamicCast<ble::BleNetDevice>(bleDevices.Get(0));
    auto peripheral = DynamicCast<ble::BleNetDevice>(bleDevices.Get(1));
    // the Wi-Fi station needs a moment to associate, so nothing is measured before that
    const Time start = Seconds(1);
    ble::BleHelper::ConnectStatically(central, peripheral, MilliSeconds(50), start);
    peripheral->GetLinkLayer()->SetReceiveCallback(MakeCallback(&OnBleReceive));

    if (avoidWifi)
    {
        // adaptive frequency hopping removes the data channels that fall inside the Wi-Fi signal
        const double wifiCentre = WifiChannelFrequencyMhz(wifiChannel);
        ble::BleChannelMap map;
        for (uint8_t channel = 0; channel < ble::N_DATA_CHANNELS; ++channel)
        {
            const double frequency = ble::ChannelToFrequencyMhz(channel);
            if (std::abs(frequency - wifiCentre) < 11.0)
            {
                map.SetUsed(channel, false);
                ++blockedChannels;
            }
        }
        // both ends of a connection must be given the same map
        central->GetLinkLayer()->SetChannelMap(map);
        peripheral->GetLinkLayer()->SetChannelMap(map);
    }

    for (uint32_t i = 0; i < 100000; ++i)
    {
        central->GetLinkLayer()->Enqueue(Create<Packet>(200),
                                         peripheral->GetLinkLayer()->GetAddress());
    }

    // the Wi-Fi link
    Ptr<PacketSink> sink;
    if (withWifi)
    {
        SpectrumWifiPhyHelper wifiPhy;
        wifiPhy.SetChannel(spectrumChannel);
        std::ostringstream channelSettings;
        channelSettings << "{" << wifiChannel << ", 20, BAND_2_4GHZ, 0}";
        wifiPhy.Set("ChannelSettings", StringValue(channelSettings.str()));
        wifiPhy.Set("TxPowerStart", DoubleValue(16.0));
        wifiPhy.Set("TxPowerEnd", DoubleValue(16.0));

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211n);
        wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                     "DataMode",
                                     StringValue("HtMcs7"),
                                     "ControlMode",
                                     StringValue("HtMcs0"));
        WifiMacHelper wifiMac;
        Ssid ssid = Ssid("ble-coexistence");
        wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
        auto staDevice = wifi.Install(wifiPhy, wifiMac, wifiNodes.Get(1));
        wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        auto apDevice = wifi.Install(wifiPhy, wifiMac, wifiNodes.Get(0));
        WifiHelper::AssignStreams(staDevice, 200);
        WifiHelper::AssignStreams(apDevice, 300);

        InternetStackHelper internet;
        internet.Install(wifiNodes);
        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        auto apInterface = address.Assign(apDevice);
        auto staInterface = address.Assign(staDevice);

        const uint16_t port = 9000;
        PacketSinkHelper sinkHelper("ns3::UdpSocketFactory",
                                    InetSocketAddress(Ipv4Address::GetAny(), port));
        auto sinkApps = sinkHelper.Install(wifiNodes.Get(1));
        sink = DynamicCast<PacketSink>(sinkApps.Get(0));
        sinkApps.Start(Seconds(0));

        OnOffHelper onoff("ns3::UdpSocketFactory",
                          InetSocketAddress(staInterface.GetAddress(0), port));
        onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        onoff.SetAttribute("DataRate", DataRateValue(DataRate("60Mbps")));
        onoff.SetAttribute("PacketSize", UintegerValue(1400));
        auto clientApps = onoff.Install(wifiNodes.Get(0));
        clientApps.Start(start);
        clientApps.Stop(start + duration);
    }

    Simulator::Stop(start + duration);
    Simulator::Run();

    bleKbps = g_bleBytes * 8.0 / duration.GetSeconds() / 1000.0;
    wifiMbps = sink ? sink->GetTotalRx() * 8.0 / duration.GetSeconds() / 1e6 : 0.0;
    Simulator::Destroy();
}

} // namespace

int
main(int argc, char* argv[])
{
    uint32_t wifiChannel = 6;
    Time duration = Seconds(2);

    CommandLine cmd(__FILE__);
    cmd.AddValue("wifiChannel", "The 2.4 GHz Wi-Fi channel number", wifiChannel);
    cmd.AddValue("duration",
                 "How long each scenario is measured, after one second of association",
                 duration);
    cmd.Parse(argc, argv);

    std::cout << "\nBLE and Wi-Fi sharing the 2.4 GHz band\n"
              << "Wi-Fi occupies 20 MHz centred on " << WifiChannelFrequencyMhz(wifiChannel)
              << " MHz (channel " << wifiChannel << ")\n\n";
    std::cout << std::left << std::setw(34) << "scenario" << std::setw(18) << "BLE throughput"
              << std::setw(18) << "Wi-Fi throughput" << "BLE channels removed\n";

    double bleKbps = 0;
    double wifiMbps = 0;
    uint32_t blocked = 0;

    Run(false, false, wifiChannel, duration, bleKbps, wifiMbps, blocked);
    const double bleAlone = bleKbps;
    std::cout << std::left << std::setw(34) << "BLE alone" << std::setw(18)
              << (std::to_string(static_cast<int>(bleKbps)) + " kb/s") << std::setw(18) << "-"
              << blocked << "\n";

    Run(true, false, wifiChannel, duration, bleKbps, wifiMbps, blocked);
    std::cout << std::left << std::setw(34) << "BLE with Wi-Fi" << std::setw(18)
              << (std::to_string(static_cast<int>(bleKbps)) + " kb/s") << std::setw(18)
              << (std::to_string(static_cast<int>(wifiMbps)) + " Mb/s") << blocked << "\n";
    const double bleWithWifi = bleKbps;

    Run(true, true, wifiChannel, duration, bleKbps, wifiMbps, blocked);
    std::cout << std::left << std::setw(34) << "BLE with Wi-Fi, channels avoided" << std::setw(18)
              << (std::to_string(static_cast<int>(bleKbps)) + " kb/s") << std::setw(18)
              << (std::to_string(static_cast<int>(wifiMbps)) + " Mb/s") << blocked << "\n";

    std::cout << "\n  BLE keeps " << std::fixed << std::setprecision(0)
              << (100.0 * bleWithWifi / std::max(bleAlone, 1.0))
              << " % of its throughput next to a saturated Wi-Fi link\n"
              << std::endl;
    return 0;
}
