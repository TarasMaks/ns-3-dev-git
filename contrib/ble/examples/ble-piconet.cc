/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file
 * @ingroup ble
 *
 * A central serving several peripherals.
 *
 * One central holds a connection with each of several peripherals. Because a device has a single
 * radio, the central interleaves the connections in time: each connection gets its own anchor
 * points, and the anchor points of the different connections are spread over the connection
 * interval so that they do not overlap. The example shows how the throughput each peripheral
 * receives falls as the piconet grows and how the aggregate stays bounded by what one radio can
 * carry.
 *
 * Example: ./ns3 run "ble-piconet --peripherals=8 --interval=40ms"
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

/// Octets each peripheral received, indexed by its address.
std::map<Mac48Address, uint64_t> g_received;

/**
 * Count a delivered payload.
 * @param address the peripheral that received it
 * @param packet the payload
 * @param from the address it came from
 */
void
OnReceive(Mac48Address address, Ptr<Packet> packet, Mac48Address from)
{
    g_received[address] += packet->GetSize();
}

} // namespace

int
main(int argc, char* argv[])
{
    uint32_t peripherals = 4;
    Time interval = MilliSeconds(40);
    Time duration = Seconds(5);
    uint32_t payload = 200;
    std::string phyName = "1M";

    CommandLine cmd(__FILE__);
    cmd.AddValue("peripherals", "Number of peripherals the central serves", peripherals);
    cmd.AddValue("interval", "Connection interval of every connection", interval);
    cmd.AddValue("duration", "How long the piconet is measured", duration);
    cmd.AddValue("payload", "Size of the payloads the central sends, in octets", payload);
    cmd.AddValue("phy", "PHY mode: 1M, 2M, S2 or S8", phyName);
    cmd.Parse(argc, argv);

    const auto mode = BlePhyModeFromString(phyName);

    // the central needs one device per connection, because a connection binds one radio
    NodeContainer centralNode;
    centralNode.Create(1);
    NodeContainer peripheralNodes;
    peripheralNodes.Create(peripherals);

    MobilityHelper mobility;
    auto positions = CreateObject<ListPositionAllocator>();
    positions->Add(Vector(0.0, 0.0, 0.0));
    for (uint32_t i = 0; i < peripherals; ++i)
    {
        const double angle = 2 * M_PI * i / peripherals;
        positions->Add(Vector(3.0 * std::cos(angle), 3.0 * std::sin(angle), 0.0));
    }
    mobility.SetPositionAllocator(positions);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(centralNode);
    mobility.Install(peripheralNodes);

    BleHelper ble;
    ble.SetChannel(BleHelper::CreateChannel("ns3::FriisPropagationLossModel"));
    ble.SetPhyAttribute("PhyMode", EnumValue(mode));
    ble.SetLinkLayerAttribute("QueueSize", UintegerValue(100000));

    // one central device per connection, all on the same node
    NetDeviceContainer centralDevices;
    for (uint32_t i = 0; i < peripherals; ++i)
    {
        centralDevices.Add(ble.Install(centralNode.Get(0)));
    }
    auto peripheralDevices = ble.Install(peripheralNodes);
    BleHelper::AssignStreams(centralDevices, 1);
    BleHelper::AssignStreams(peripheralDevices, 1000);

    const Time start = MilliSeconds(10);
    for (uint32_t i = 0; i < peripherals; ++i)
    {
        auto central = DynamicCast<BleNetDevice>(centralDevices.Get(i));
        auto peripheral = DynamicCast<BleNetDevice>(peripheralDevices.Get(i));
        // the anchor points of the connections are spread over the connection interval so that
        // the single radio of the central is not asked to be in two places at once
        const Time offset = interval * i / peripherals;
        BleHelper::ConnectStatically(central, peripheral, interval, start + offset);

        const auto address = peripheral->GetLinkLayer()->GetAddress();
        peripheral->GetLinkLayer()->SetReceiveCallback(MakeBoundCallback(&OnReceive, address));
        g_received[address] = 0;

        for (uint32_t p = 0; p < 20000; ++p)
        {
            central->GetLinkLayer()->Enqueue(Create<Packet>(payload), address);
        }
    }

    Simulator::Stop(start + duration);
    Simulator::Run();

    std::cout << "\nA central serving " << peripherals << " peripherals on " << phyName
              << ", connection interval " << interval.As(Time::MS) << "\n\n";
    std::cout << std::left << std::setw(24) << "peripheral" << "throughput\n";
    double aggregate = 0;
    for (const auto& [address, bytes] : g_received)
    {
        const double kbps = bytes * 8.0 / duration.GetSeconds() / 1000.0;
        aggregate += kbps;
        std::ostringstream label;
        label << address;
        std::cout << std::left << std::setw(24) << label.str() << std::fixed << std::setprecision(1)
                  << kbps << " kb/s\n";
    }
    std::cout << std::left << std::setw(24) << "aggregate" << std::fixed << std::setprecision(1)
              << aggregate << " kb/s\n"
              << std::endl;
    Simulator::Destroy();
    return 0;
}
