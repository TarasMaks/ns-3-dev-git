/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * UWB and Wi-Fi 6E in the same half gigahertz.
 *
 * UWB channel 5 runs from 6240 to 6739 MHz. Wi-Fi 6E channel 103 is an 80 MHz channel centred
 * at 6465 MHz, squarely inside it. They are not neighbours that leak into one another; they
 * occupy the same spectrum, and every UWB deployment in a modern building has to live with it.
 *
 * The two radios are wildly unequal. The Wi-Fi transmitter radiates about 16 dBm; the UWB
 * transmitter is held by regulators to -14 dBm across its whole channel, thirty dB less, spread
 * over six times the bandwidth. On a power basis UWB should lose badly. What saves it is
 * processing gain: the receiver correlates over a channel thousands of times wider than the
 * information rate, so an interferer that occupies a sixth of the band and is thirty dB stronger
 * still leaves the link working.
 *
 * The example measures both directions: what Wi-Fi does to UWB ranging, and what UWB does to
 * Wi-Fi throughput. Both share one spectrum channel, so the interference is computed from the
 * real power spectral densities rather than assumed.
 */

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/propagation-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/uwb-module.h"
#include "ns3/wifi-module.h"

#include <cmath>
#include <iomanip>
#include <iostream>
#include <vector>

using namespace ns3;
using namespace ns3::uwb;

NS_LOG_COMPONENT_DEFINE("UwbWifiCoexistence");

namespace
{

std::vector<double> g_ranges; //!< every range measured in the run in progress
uint32_t g_attempts = 0;      //!< exchanges started
uint32_t g_failed = 0;        //!< exchanges that did not complete

/// @param result what an exchange produced
void
OnResult(const UwbRangingResult& result)
{
    if (!result.valid)
    {
        ++g_failed;
        return;
    }
    if (!result.measuredHere)
    {
        return;
    }
    g_ranges.push_back(result.rangeMetres);
}

/// What one run of the experiment produced.
struct Outcome
{
    double completed{0.0};    //!< the fraction of exchanges that produced a range
    double bias{0.0};         //!< the average error of those ranges, in metres
    double sigma{0.0};        //!< their spread, in metres
    double wifiMbps{0.0};     //!< what the Wi-Fi flow carried
};

/**
 * Run one experiment.
 *
 * @param uwbOn whether the UWB pair runs
 * @param wifiOn whether the Wi-Fi pair runs
 * @param wifiChannel the 6 GHz Wi-Fi channel number
 * @param separation how far the UWB pair is from the Wi-Fi pair, in metres
 * @param seconds how long the run lasts
 * @return what was measured
 */
Outcome
Run(bool uwbOn, bool wifiOn, uint32_t wifiChannel, double separation, double seconds)
{
    g_ranges.clear();
    g_attempts = 0;
    g_failed = 0;

    const double uwbDistance = 10.0;
    const uint8_t uwbChannel = 5;

    // one channel carries everything, so the two technologies interfere through their real
    // power spectral densities rather than by assumption
    auto spectrumChannel = UwbHelper::CreateChannel(uwbChannel);

    NodeContainer uwbNodes;
    uwbNodes.Create(2);
    NodeContainer wifiNodes;
    wifiNodes.Create(2);

    MobilityHelper mobility;
    auto positions = CreateObject<ListPositionAllocator>();
    positions->Add(Vector(0, 0, 0));
    positions->Add(Vector(uwbDistance, 0, 0));
    positions->Add(Vector(0, separation, 0));
    positions->Add(Vector(uwbDistance, separation, 0));
    mobility.SetPositionAllocator(positions);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    NodeContainer all;
    all.Add(uwbNodes);
    all.Add(wifiNodes);
    mobility.Install(all);

    Ptr<UwbNetDevice> initiator;
    Ptr<UwbNetDevice> responder;
    if (uwbOn)
    {
        UwbHelper uwb;
        uwb.SetChannelNumber(uwbChannel);
        uwb.SetDataRate(UwbDataRate::RATE_6M81);
        uwb.SetPreambleSymbols(128);
        uwb.SetChannel(spectrumChannel);
        auto devices = uwb.Install(uwbNodes);
        uwb.AssignStreams(devices, 1);
        initiator = DynamicCast<UwbNetDevice>(devices.Get(0));
        responder = DynamicCast<UwbNetDevice>(devices.Get(1));
        initiator->SetRangingResultCallback(MakeCallback(&OnResult));
        responder->SetRangingResultCallback(MakeCallback(&OnResult));
    }

    double wifiMbps = 0.0;
    Ptr<PacketSink> sink;
    if (wifiOn)
    {
        SpectrumWifiPhyHelper wifiPhy;
        wifiPhy.SetChannel(spectrumChannel);
        std::ostringstream channelSettings;
        channelSettings << "{" << wifiChannel << ", 80, BAND_6GHZ, 0}";
        wifiPhy.Set("ChannelSettings", StringValue(channelSettings.str()));
        wifiPhy.Set("TxPowerStart", DoubleValue(16.0));
        wifiPhy.Set("TxPowerEnd", DoubleValue(16.0));

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211ax);
        wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                     "DataMode",
                                     StringValue("HeMcs9"),
                                     "ControlMode",
                                     StringValue("HeMcs0"));
        WifiMacHelper wifiMac;
        Ssid ssid = Ssid("uwb-coexistence");
        wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
        auto staDevice = wifi.Install(wifiPhy, wifiMac, wifiNodes.Get(1));
        wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        auto apDevice = wifi.Install(wifiPhy, wifiMac, wifiNodes.Get(0));
        WifiHelper::AssignStreams(staDevice, 200);
        WifiHelper::AssignStreams(apDevice, 300);

        InternetStackHelper internet;
        internet.Install(wifiNodes);
        Ipv4AddressHelper addresses;
        addresses.SetBase("10.1.1.0", "255.255.255.0");
        NetDeviceContainer wifiDevices;
        wifiDevices.Add(apDevice);
        wifiDevices.Add(staDevice);
        auto interfaces = addresses.Assign(wifiDevices);

        const uint16_t port = 9000;
        PacketSinkHelper sinkHelper("ns3::UdpSocketFactory",
                                    InetSocketAddress(Ipv4Address::GetAny(), port));
        auto sinkApps = sinkHelper.Install(wifiNodes.Get(1));
        sinkApps.Start(Seconds(0.5));
        sink = DynamicCast<PacketSink>(sinkApps.Get(0));

        OnOffHelper source("ns3::UdpSocketFactory",
                           InetSocketAddress(interfaces.GetAddress(1), port));
        source.SetConstantRate(DataRate("200Mbps"), 1400);
        auto sourceApps = source.Install(wifiNodes.Get(0));
        sourceApps.Start(Seconds(1.0));
        sourceApps.Stop(Seconds(1.0 + seconds));
    }

    if (uwbOn)
    {
        const Mac16Address peer = Mac16Address::ConvertFrom(responder->GetAddress());
        const Time spacing = MilliSeconds(20);
        const auto exchanges = static_cast<uint32_t>(seconds / spacing.GetSeconds());
        for (uint32_t i = 0; i < exchanges; ++i)
        {
            ++g_attempts;
            Simulator::Schedule(Seconds(1.0) + spacing * (i + 1), [initiator, peer]() {
                initiator->StartRanging(peer, UwbRangingMethod::DS_TWR);
            });
        }
    }

    Simulator::Stop(Seconds(1.5 + seconds));
    Simulator::Run();
    if (sink)
    {
        wifiMbps = sink->GetTotalRx() * 8.0 / seconds / 1e6;
    }
    Simulator::Destroy();

    Outcome outcome;
    outcome.wifiMbps = wifiMbps;
    outcome.completed = (g_attempts == 0) ? 0.0
                                          : static_cast<double>(g_ranges.size()) / g_attempts;
    if (!g_ranges.empty())
    {
        double mean = 0.0;
        for (const double range : g_ranges)
        {
            mean += range / g_ranges.size();
        }
        double variance = 0.0;
        for (const double range : g_ranges)
        {
            variance += (range - mean) * (range - mean) / g_ranges.size();
        }
        outcome.bias = mean - uwbDistance;
        outcome.sigma = std::sqrt(variance);
    }
    return outcome;
}

/**
 * Print one row of the table.
 *
 * @param label what the row is
 * @param outcome what was measured
 * @param uwbOn whether the UWB pair ran
 * @param wifiOn whether the Wi-Fi pair ran
 */
void
PrintRow(const std::string& label, const Outcome& outcome, bool uwbOn, bool wifiOn)
{
    std::cout << std::left << std::setw(30) << label << std::right << std::fixed;
    if (uwbOn)
    {
        std::cout << std::setprecision(1) << std::setw(12) << outcome.completed * 100 << "%"
                  << std::setprecision(2) << std::setw(12) << outcome.bias * 100 << std::setw(12)
                  << outcome.sigma * 100;
    }
    else
    {
        std::cout << std::setw(13) << "-" << std::setw(12) << "-" << std::setw(12) << "-";
    }
    if (wifiOn)
    {
        std::cout << std::setprecision(1) << std::setw(13) << outcome.wifiMbps;
    }
    else
    {
        std::cout << std::setw(13) << "-";
    }
    std::cout << "\n";
}

} // namespace

int
main(int argc, char* argv[])
{
    uint32_t wifiChannel = 103;
    double seconds = 2.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("wifiChannel",
                 "The 6 GHz Wi-Fi channel: 103 sits inside UWB channel 5, 7 is well below it",
                 wifiChannel);
    cmd.AddValue("seconds", "How long each run lasts", seconds);
    cmd.Parse(argc, argv);

    std::cout << "\nUWB channel 5 covers " << ChannelToFrequencyMhz(5) - 249.6 << " to "
              << ChannelToFrequencyMhz(5) + 249.6 << " MHz at " << std::fixed
              << std::setprecision(1) << GetRegulatoryTxPowerDbm(5) << " dBm in total.\n"
              << "The Wi-Fi 6E pair is on channel " << wifiChannel
              << ", 80 MHz wide, at 16 dBm.\n"
              << "The UWB pair is 10 m apart and ranges fifty times a second.\n\n"
              << std::left << std::setw(30) << "configuration" << std::right << std::setw(13)
              << "completed" << std::setw(12) << "bias cm" << std::setw(12) << "sigma cm"
              << std::setw(13) << "Wi-Fi Mb/s" << "\n"
              << std::string(80, '-') << "\n";

    PrintRow("UWB alone", Run(true, false, wifiChannel, 10.0, seconds), true, false);
    PrintRow("Wi-Fi alone", Run(false, true, wifiChannel, 10.0, seconds), false, true);
    std::cout << "\n";

    for (const double separation : {2.0, 5.0, 20.0, 50.0, 100.0, 200.0})
    {
        std::ostringstream label;
        label << "both, Wi-Fi " << separation << " m away";
        PrintRow(label.str(), Run(true, true, wifiChannel, separation, seconds), true, true);
    }

    std::cout << "\nThe processing gain of the UWB receiver is 18.65 dB at 6.81 Mb/s, and that\n"
                 "is a great deal, but it is not thirty. A Wi-Fi 6E radio transmits about\n"
                 "thirty dB more power than a UWB one is allowed to, so at close range it\n"
                 "simply buries the link, and the UWB pair needs the Wi-Fi pair to be roughly\n"
                 "an order of magnitude further away than it is from its own peer before the\n"
                 "gain is enough. That is a real constraint on deploying UWB in a building\n"
                 "with 6 GHz Wi-Fi, and it is why the choice of UWB channel matters: run this\n"
                 "again with --wifiChannel=7, which sits at 5985 MHz and well outside UWB\n"
                 "channel 5, and the interference disappears entirely.\n\n"
                 "The other direction is not symmetric. The Wi-Fi throughput hardly moves\n"
                 "whatever the UWB pair does, because a transmitter held to -41.3 dBm per\n"
                 "megahertz puts less power into the 80 MHz Wi-Fi channel than that channel's\n"
                 "own noise floor. UWB is a quiet neighbour by regulation, not by courtesy.\n\n";
    return 0;
}
