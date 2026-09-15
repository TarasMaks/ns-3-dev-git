/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file
 * @ingroup ble
 *
 * Discovery latency of the advertising and scanning procedures.
 *
 * A number of advertisers send undirected advertising events on the three primary advertising
 * channels while one scanner listens for a scan window inside every scan interval, rotating
 * through the same three channels. The example measures how long the scanner takes to hear each
 * advertiser for the first time and how that depends on the advertising interval and on the
 * duty cycle of the scanner.
 *
 * A scanner that listens continuously on one channel hears an advertiser after about one
 * advertising interval, because every event visits all three channels. A scanner with a duty
 * cycle of d takes roughly that long divided by d, and advertisers collide with one another as
 * their number grows, which the random delay the specification adds to every advertising event
 * is there to break up.
 *
 * Example: ./ns3 run "ble-advertising-discovery --advertisers=20 --scanWindow=10ms"
 */

#include "ns3/ble-helper.h"
#include "ns3/core-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"

#include <iomanip>
#include <map>
#include <vector>

using namespace ns3;
using namespace ns3::ble;

namespace
{

/// When each advertiser was first heard.
std::map<Mac48Address, Time> g_firstHeard;
/// How many advertising reports the scanner produced.
uint32_t g_reports = 0;

/**
 * Record an advertising report.
 * @param report the report
 */
void
OnAdvReport(BleAdvReport report)
{
    ++g_reports;
    if (!g_firstHeard.count(report.address))
    {
        g_firstHeard[report.address] = Simulator::Now();
    }
}

} // namespace

int
main(int argc, char* argv[])
{
    uint32_t advertisers = 5;
    Time advInterval = MilliSeconds(100);
    Time scanInterval = MilliSeconds(100);
    Time scanWindow = MilliSeconds(30);
    Time duration = Seconds(10);
    double radius = 5.0;
    bool activeScanning = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("advertisers", "Number of advertising devices", advertisers);
    cmd.AddValue("advInterval", "Interval between advertising events", advInterval);
    cmd.AddValue("scanInterval", "Interval between the starts of two scan windows", scanInterval);
    cmd.AddValue("scanWindow",
                 "How long the scanner listens inside each scan interval",
                 scanWindow);
    cmd.AddValue("activeScanning", "Whether the scanner requests scan responses", activeScanning);
    cmd.AddValue("duration", "How long the scan runs", duration);
    cmd.AddValue("radius", "Radius of the circle the advertisers sit on, in metres", radius);
    cmd.Parse(argc, argv);

    NodeContainer scannerNode;
    scannerNode.Create(1);
    NodeContainer advertiserNodes;
    advertiserNodes.Create(advertisers);

    MobilityHelper mobility;
    auto positions = CreateObject<ListPositionAllocator>();
    positions->Add(Vector(0.0, 0.0, 0.0));
    for (uint32_t i = 0; i < advertisers; ++i)
    {
        const double angle = 2 * M_PI * i / advertisers;
        positions->Add(Vector(radius * std::cos(angle), radius * std::sin(angle), 0.0));
    }
    mobility.SetPositionAllocator(positions);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(scannerNode);
    mobility.Install(advertiserNodes);

    BleHelper ble;
    ble.SetChannel(BleHelper::CreateChannel("ns3::FriisPropagationLossModel"));
    ble.SetLinkLayerAttribute("AdvInterval", TimeValue(advInterval));
    ble.SetLinkLayerAttribute("ScanInterval", TimeValue(scanInterval));
    ble.SetLinkLayerAttribute("ScanWindow", TimeValue(scanWindow));
    ble.SetLinkLayerAttribute("ActiveScanning", BooleanValue(activeScanning));

    auto scannerDevices = ble.Install(scannerNode);
    auto advertiserDevices = ble.Install(advertiserNodes);
    BleHelper::AssignStreams(scannerDevices, 1);
    BleHelper::AssignStreams(advertiserDevices, 100);

    auto scanner = DynamicCast<BleNetDevice>(scannerDevices.Get(0));
    scanner->Initialize();
    scanner->GetLinkLayer()->TraceConnectWithoutContext("AdvReport", MakeCallback(&OnAdvReport));

    const Time start = MilliSeconds(1);
    Simulator::Schedule(start, &BleLinkLayer::StartScanning, scanner->GetLinkLayer());
    for (uint32_t i = 0; i < advertisers; ++i)
    {
        auto device = DynamicCast<BleNetDevice>(advertiserDevices.Get(i));
        device->Initialize();
        Simulator::Schedule(start, &BleLinkLayer::StartAdvertising, device->GetLinkLayer());
    }

    Simulator::Stop(duration);
    Simulator::Run();

    const double dutyCycle = std::min(1.0, scanWindow.GetDouble() / scanInterval.GetDouble());
    std::cout << "\nDiscovery of " << advertisers << " advertisers by one scanner\n"
              << "  advertising interval  " << advInterval.As(Time::MS) << "\n"
              << "  scan window           " << scanWindow.As(Time::MS) << " inside "
              << scanInterval.As(Time::MS) << ", duty cycle " << std::fixed << std::setprecision(2)
              << dutyCycle << "\n"
              << "  active scanning       " << (activeScanning ? "yes" : "no") << "\n\n";

    std::cout << "  advertisers discovered " << g_firstHeard.size() << " of " << advertisers
              << "\n  advertising reports    " << g_reports << "\n";

    if (!g_firstHeard.empty())
    {
        double sum = 0;
        double worst = 0;
        for (const auto& [address, when] : g_firstHeard)
        {
            const double latency = (when - start).GetSeconds() * 1000.0;
            sum += latency;
            worst = std::max(worst, latency);
        }
        std::cout << "  mean discovery latency " << std::fixed << std::setprecision(1)
                  << (sum / g_firstHeard.size()) << " ms\n"
                  << "  worst case             " << worst << " ms\n";
        // a scanner that listens for a fraction d of the time needs about one advertising
        // interval divided by d to catch an advertiser
        std::cout << "  rough expectation      "
                  << (advInterval.GetSeconds() * 1000.0 / std::max(dutyCycle, 1e-9)) << " ms\n";
    }
    std::cout << std::endl;
    Simulator::Destroy();
    return 0;
}
