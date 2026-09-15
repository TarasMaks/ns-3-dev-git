/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file
 * @ingroup ble
 *
 * Range of the four LE PHYs.
 *
 * A central and a peripheral are moved apart and the fraction of payloads that arrive is
 * measured for each PHY mode. The coded PHYs trade data rate for sensitivity, so they keep a
 * connection alive well beyond the range of LE 1M, which is the reason the Bluetooth Core
 * Specification introduced them; LE 2M pays three decibels for its wider bandwidth and is the
 * first to fail.
 *
 * The example also prints, for each mode, the sensitivity the error model produces and the
 * distance at which the received power reaches it under the free space path loss used here, so
 * that the simulated range can be checked against the link budget.
 *
 * Example: ./ns3 run "ble-range-phy-modes --txPower=0 --distances=10,50,100,200"
 */

#include "ns3/ble-helper.h"
#include "ns3/core-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"

#include <cmath>
#include <iomanip>
#include <sstream>
#include <vector>

using namespace ns3;
using namespace ns3::ble;

namespace
{

/// Payloads delivered to the peripheral.
uint32_t g_received = 0;

/**
 * Count a delivered payload.
 * @param packet the payload
 * @param from the address it came from
 */
void
OnReceive(Ptr<Packet> packet, Mac48Address from)
{
    ++g_received;
}

/**
 * @param s a comma separated list of numbers
 * @return the numbers
 */
std::vector<double>
ParseList(const std::string& s)
{
    std::vector<double> values;
    std::stringstream stream(s);
    std::string item;
    while (std::getline(stream, item, ','))
    {
        if (!item.empty())
        {
            values.push_back(std::stod(item));
        }
    }
    return values;
}

/**
 * Run one connection at a distance and measure how many payloads arrive.
 *
 * @param mode the PHY mode
 * @param distance the distance between the devices, in metres
 * @param txPowerDbm the transmit power
 * @param sent filled with the number of payloads offered
 * @return the fraction of payloads that arrived
 */
double
RunOne(BlePhyMode mode, double distance, double txPowerDbm, uint32_t& sent)
{
    g_received = 0;

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
    ble.SetPhyAttribute("TxPower", DoubleValue(txPowerDbm));
    auto devices = ble.Install(nodes);
    BleHelper::AssignStreams(devices, 7);

    auto central = DynamicCast<BleNetDevice>(devices.Get(0));
    auto peripheral = DynamicCast<BleNetDevice>(devices.Get(1));
    const Time start = MilliSeconds(10);
    BleHelper::ConnectStatically(central, peripheral, MilliSeconds(20), start);
    peripheral->GetLinkLayer()->SetReceiveCallback(MakeCallback(&OnReceive));

    sent = 100;
    for (uint32_t i = 0; i < sent; ++i)
    {
        Simulator::Schedule(start + MilliSeconds(20 * i), [central, peripheral]() {
            central->GetLinkLayer()->Enqueue(Create<Packet>(20),
                                             peripheral->GetLinkLayer()->GetAddress());
        });
    }

    Simulator::Stop(start + MilliSeconds(20 * (sent + 20)));
    Simulator::Run();
    Simulator::Destroy();
    return static_cast<double>(g_received) / sent;
}

/**
 * The distance at which free space path loss brings a transmission down to a power level.
 *
 * @param txPowerDbm the transmit power
 * @param rxPowerDbm the received power
 * @param frequencyHz the carrier frequency
 * @return the distance, in metres
 */
double
FreeSpaceRange(double txPowerDbm, double rxPowerDbm, double frequencyHz)
{
    const double lossDb = txPowerDbm - rxPowerDbm;
    const double wavelength = 299792458.0 / frequencyHz;
    return wavelength / (4 * M_PI) * std::pow(10.0, lossDb / 20.0);
}

} // namespace

int
main(int argc, char* argv[])
{
    double txPowerDbm = 0.0;
    std::string distancesStr = "10,25,50,100,200,400";

    CommandLine cmd(__FILE__);
    cmd.AddValue("txPower", "Transmit power of both devices, in dBm", txPowerDbm);
    cmd.AddValue("distances", "Comma separated list of distances, in metres", distancesStr);
    cmd.Parse(argc, argv);

    const auto distances = ParseList(distancesStr);
    const std::vector<BlePhyMode> modes{BlePhyMode::LE_1M,
                                        BlePhyMode::LE_2M,
                                        BlePhyMode::LE_CODED_S2,
                                        BlePhyMode::LE_CODED_S8};

    auto reference = CreateObject<BlePhy>();
    reference->SetRfChannel(ADV_CHANNEL_37);

    std::cout << "\nRange of the four LE PHYs, transmitting at " << txPowerDbm << " dBm\n\n";
    std::cout << std::left << std::setw(14) << "PHY" << std::setw(16) << "sensitivity"
              << std::setw(22) << "free space range" << "delivery ratio at each distance\n";
    std::cout << std::left << std::setw(52) << "";
    for (const auto distance : distances)
    {
        std::ostringstream header;
        header << distance << " m";
        std::cout << std::setw(10) << header.str();
    }
    std::cout << "\n";

    for (const auto mode : modes)
    {
        const double sensitivity = reference->CalculateSensitivityDbm(mode);
        const double range = FreeSpaceRange(txPowerDbm, sensitivity, 2.402e9);
        std::cout << std::left << std::setw(14) << BlePhyModeName(mode) << std::setw(16)
                  << (std::to_string(static_cast<int>(std::round(sensitivity))) + " dBm")
                  << std::setw(22) << (std::to_string(static_cast<int>(std::round(range))) + " m");
        for (const auto distance : distances)
        {
            uint32_t sent = 0;
            const double ratio = RunOne(mode, distance, txPowerDbm, sent);
            std::ostringstream cell;
            cell << std::fixed << std::setprecision(2) << ratio;
            std::cout << std::setw(10) << cell.str();
        }
        std::cout << "\n";
    }
    std::cout << std::endl;
    return 0;
}
