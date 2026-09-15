/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file
 * @ingroup ble
 *
 * Throughput of a single BLE connection.
 *
 * A central and a peripheral one metre apart exchange as much data as the connection can carry.
 * The example sweeps the PHY mode, the connection interval and the data channel PDU payload, and
 * compares the measured application throughput with the analytical maximum obtained by filling
 * a connection event with exchanges of a full data PDU answered by an empty one:
 *
 *     throughput = payload * 8 / (T_data + T_IFS + T_empty + T_IFS)
 *
 * The figures usually quoted for a connection using the Data Length Extension, around 800 kb/s
 * on LE 1M and 1.4 Mb/s on LE 2M, come out of this expression.
 *
 * Example: ./ns3 run "ble-connection-throughput --phy=2M --payload=251"
 */

#include "ns3/ble-helper.h"
#include "ns3/core-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"

#include <iomanip>
#include <sstream>
#include <vector>

using namespace ns3;
using namespace ns3::ble;

namespace
{

/// Total octets delivered to the peripheral.
uint64_t g_rxBytes = 0;
/// Data channel PDUs the central sent.
uint64_t g_pdus = 0;
/// Data channel PDUs the central had to send again.
uint64_t g_retransmissions = 0;

/**
 * Count a delivered payload.
 * @param packet the payload
 * @param from the address it came from
 */
void
OnReceive(Ptr<Packet> packet, Mac48Address from)
{
    g_rxBytes += packet->GetSize();
}

/**
 * Count a data channel PDU.
 * @param length the payload length of the PDU
 * @param retransmission whether the PDU had already been sent
 */
void
OnPduSent(uint8_t length, bool retransmission)
{
    ++g_pdus;
    if (retransmission)
    {
        ++g_retransmissions;
    }
}

/**
 * The largest application throughput a connection can carry.
 *
 * @param mode the PHY mode
 * @param payload the data channel PDU payload, in octets
 * @return the throughput in kb/s
 */
double
AnalyticalMaxKbps(BlePhyMode mode, uint32_t payload)
{
    const double dataUs =
        BlePhy::CalculateTxDuration(PDU_HEADER_OCTETS + payload, mode).GetNanoSeconds() / 1000.0;
    const double emptyUs =
        BlePhy::CalculateTxDuration(PDU_HEADER_OCTETS, mode).GetNanoSeconds() / 1000.0;
    return (payload * 8.0) / (dataUs + T_IFS_US + emptyUs + T_IFS_US) * 1000.0;
}

/**
 * Run one connection and measure its throughput.
 *
 * @param mode the PHY mode
 * @param interval the connection interval
 * @param payload the data channel PDU payload, in octets
 * @param distance the distance between the two devices, in metres
 * @param duration how long the connection is measured
 * @return the measured throughput in kb/s
 */
double
RunOne(BlePhyMode mode, Time interval, uint32_t payload, double distance, Time duration)
{
    g_rxBytes = 0;
    g_pdus = 0;
    g_retransmissions = 0;

    NodeContainer nodes;
    nodes.Create(2);
    MobilityHelper mobility;
    auto positions = CreateObject<ListPositionAllocator>();
    positions->Add(Vector(0.0, 0.0, 0.0));
    positions->Add(Vector(distance, 0.0, 0.0));
    mobility.SetPositionAllocator(positions);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    BleHelper ble;
    ble.SetChannel(BleHelper::CreateChannel("ns3::FriisPropagationLossModel"));
    ble.SetPhyAttribute("PhyMode", EnumValue(mode));
    ble.SetLinkLayerAttribute("MaxPduPayload", UintegerValue(payload));
    ble.SetLinkLayerAttribute("QueueSize", UintegerValue(200000));
    auto devices = ble.Install(nodes);
    BleHelper::AssignStreams(devices, 1);

    auto central = DynamicCast<BleNetDevice>(devices.Get(0));
    auto peripheral = DynamicCast<BleNetDevice>(devices.Get(1));
    const Time start = MilliSeconds(10);
    BleHelper::ConnectStatically(central, peripheral, interval, start);

    peripheral->GetLinkLayer()->SetReceiveCallback(MakeCallback(&OnReceive));
    central->GetLinkLayer()->TraceConnectWithoutContext("PduSent", MakeCallback(&OnPduSent));

    // keep the queue full so that the connection, not the application, is the limit
    const uint32_t sduSize = payload - L2CAP_HEADER_OCTETS;
    const auto offered = static_cast<uint32_t>(2.0 * AnalyticalMaxKbps(mode, payload) * 1000.0 *
                                               duration.GetSeconds() / (sduSize * 8));
    for (uint32_t i = 0; i < offered + 100; ++i)
    {
        central->GetLinkLayer()->Enqueue(Create<Packet>(sduSize),
                                         peripheral->GetLinkLayer()->GetAddress());
    }

    Simulator::Stop(start + duration);
    Simulator::Run();
    const double kbps = g_rxBytes * 8.0 / duration.GetSeconds() / 1000.0;
    Simulator::Destroy();
    return kbps;
}

} // namespace

int
main(int argc, char* argv[])
{
    std::string phyName;
    uint32_t payload = 0;
    double distance = 1.0;
    Time duration = Seconds(2);
    Time interval = Time(0);

    CommandLine cmd(__FILE__);
    cmd.AddValue("phy", "Limit the sweep to one PHY: 1M, 2M, S2 or S8", phyName);
    cmd.AddValue("payload", "Limit the sweep to one data channel PDU payload, in octets", payload);
    cmd.AddValue("interval", "Limit the sweep to one connection interval", interval);
    cmd.AddValue("distance", "Distance between the two devices, in metres", distance);
    cmd.AddValue("duration", "How long each connection is measured", duration);
    cmd.Parse(argc, argv);

    std::vector<BlePhyMode> modes{BlePhyMode::LE_1M,
                                  BlePhyMode::LE_2M,
                                  BlePhyMode::LE_CODED_S2,
                                  BlePhyMode::LE_CODED_S8};
    if (!phyName.empty())
    {
        modes = {BlePhyModeFromString(phyName)};
    }
    std::vector<uint32_t> payloads{DATA_PDU_PAYLOAD_MIN_MAX, 100, DATA_PDU_PAYLOAD_MAX};
    if (payload > 0)
    {
        payloads = {payload};
    }
    std::vector<Time> intervals{MilliSeconds(7.5), MilliSeconds(30), MilliSeconds(100)};
    if (interval.IsStrictlyPositive())
    {
        intervals = {interval};
    }

    std::cout << "\nThroughput of a single BLE connection\n"
              << "The analytical maximum fills a connection event with exchanges of a full data "
                 "PDU\nanswered by an empty one.\n\n";
    std::cout << std::left << std::setw(12) << "PHY" << std::setw(10) << "payload" << std::setw(12)
              << "interval" << std::setw(14) << "measured" << std::setw(14) << "analytical"
              << std::setw(12) << "efficiency" << std::setw(10) << "PDUs" << "retransmissions\n";

    for (const auto& mode : modes)
    {
        for (const auto pduPayload : payloads)
        {
            for (const auto& connInterval : intervals)
            {
                const double measured = RunOne(mode, connInterval, pduPayload, distance, duration);
                const double analytical = AnalyticalMaxKbps(mode, pduPayload);
                std::ostringstream intervalStr;
                intervalStr << connInterval.GetMilliSeconds() << " ms";
                std::cout << std::left << std::setw(12) << BlePhyModeName(mode) << std::setw(10)
                          << pduPayload << std::setw(12) << intervalStr.str() << std::setw(14)
                          << (std::to_string(static_cast<int>(measured)) + " kb/s") << std::setw(14)
                          << (std::to_string(static_cast<int>(analytical)) + " kb/s")
                          << std::setw(12)
                          << (std::to_string(static_cast<int>(100 * measured / analytical)) + " %")
                          << std::setw(10) << g_pdus << g_retransmissions << "\n";
            }
        }
    }
    std::cout << std::endl;
    return 0;
}
