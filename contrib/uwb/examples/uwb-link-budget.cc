/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * How far a UWB link reaches, and what the choice of data rate and preamble buys.
 *
 * A UWB transmitter is held to -41.3 dBm per megahertz by regulators everywhere, which across a
 * 499.2 MHz channel is -14.3 dBm in total, three hundred times less than a Wi-Fi radio. What it
 * gets in return is processing gain: the channel is thousands of times wider than the
 * information rate, so a signal ten dB below the noise floor is still readable.
 *
 * The example measures the packet delivery of a link as the distance grows, for each of the four
 * data rates and for two preamble lengths, and prints where each configuration gives out. It
 * also reports the modelled receiver sensitivity, which is calibrated against the figures
 * published for a common transceiver, so the ranges can be checked against the link budget by
 * hand.
 */

#include "ns3/core-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/uwb-module.h"

#include <cmath>
#include <iomanip>
#include <iostream>

using namespace ns3;
using namespace ns3::uwb;

NS_LOG_COMPONENT_DEFINE("UwbLinkBudget");

namespace
{

uint32_t g_received = 0; //!< frames delivered in the run in progress
double g_rxPowerDbm = 0; //!< power of the last frame that arrived

/// @param packet the frame
/// @param source who sent it
void
OnReceive(Ptr<Packet> packet, Mac16Address source)
{
    ++g_received;
}

/// @param packet the frame
/// @param info what the receiver learned
void
OnPhyRx(Ptr<const Packet> packet, const UwbRxInfo& info)
{
    g_rxPowerDbm = info.rxPowerDbm;
}

/**
 * Send a run of frames across a link of a given length and count what arrives.
 *
 * @param distance the separation, in metres
 * @param rate the data rate
 * @param preambleSymbols the preamble length
 * @param prf the mean pulse repetition frequency
 * @param frames how many frames to send
 * @param channelNumber the UWB channel
 * @return the fraction of frames delivered
 */
double
Deliver(double distance,
        UwbDataRate rate,
        uint32_t preambleSymbols,
        UwbPrf prf,
        uint32_t frames,
        uint8_t channelNumber)
{
    g_received = 0;

    NodeContainer nodes;
    nodes.Create(2);

    MobilityHelper mobility;
    auto positions = CreateObject<ListPositionAllocator>();
    positions->Add(Vector(0, 0, 0));
    positions->Add(Vector(distance, 0, 0));
    mobility.SetPositionAllocator(positions);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    UwbHelper uwb;
    uwb.SetChannelNumber(channelNumber);
    uwb.SetDataRate(rate);
    uwb.SetPreambleSymbols(preambleSymbols);
    uwb.SetPrf(prf);
    // acknowledgements would hide a one way loss behind a retry, and what is wanted here is the
    // raw reach of the link
    uwb.SetMacAttribute("AckEnabled", BooleanValue(false));
    uwb.SetChannel(UwbHelper::CreateChannel(channelNumber));

    auto devices = uwb.Install(nodes);
    uwb.AssignStreams(devices, 1);

    auto sender = DynamicCast<UwbNetDevice>(devices.Get(0));
    auto receiver = DynamicCast<UwbNetDevice>(devices.Get(1));
    receiver->GetMac()->SetReceiveCallback(MakeCallback(&OnReceive));
    receiver->GetPhy()->TraceConnectWithoutContext("PhyRxEnd", MakeCallback(&OnPhyRx));

    const Address destination = receiver->GetAddress();
    const Time spacing = sender->GetPhy()->CalculateTxDuration(MAX_PSDU_OCTETS) + MilliSeconds(1);
    for (uint32_t i = 0; i < frames; ++i)
    {
        Simulator::Schedule(spacing * (i + 1), [sender, destination]() {
            sender->Send(Create<Packet>(100), destination, 0);
        });
    }

    Simulator::Stop(spacing * (frames + 3));
    Simulator::Run();
    Simulator::Destroy();
    return static_cast<double>(g_received) / frames;
}

/**
 * Find the distance at which a configuration stops delivering, by bisection.
 *
 * @param rate the data rate
 * @param preambleSymbols the preamble length
 * @param prf the mean pulse repetition frequency
 * @param frames how many frames to send at each distance
 * @param channelNumber the UWB channel
 * @return the greatest distance that still delivers nine frames in ten
 */
double
FindRange(UwbDataRate rate,
          uint32_t preambleSymbols,
          UwbPrf prf,
          uint32_t frames,
          uint8_t channelNumber)
{
    double low = 1.0;
    double high = 2000.0;
    for (int step = 0; step < 12; ++step)
    {
        const double middle = 0.5 * (low + high);
        if (Deliver(middle, rate, preambleSymbols, prf, frames, channelNumber) >= 0.9)
        {
            low = middle;
        }
        else
        {
            high = middle;
        }
    }
    return low;
}

} // namespace

int
main(int argc, char* argv[])
{
    uint32_t frames = 40;
    uint8_t channelNumber = 5;

    CommandLine cmd(__FILE__);
    cmd.AddValue("frames", "How many frames are sent at each distance", frames);
    cmd.AddValue("channel", "The UWB channel to use", channelNumber);
    cmd.Parse(argc, argv);

    UwbPhyConfig config;
    config.channel = channelNumber;
    auto errorModel = CreateObject<UwbErrorModel>();

    std::cout << "\nChannel " << +channelNumber << ", " << ChannelToFrequencyMhz(channelNumber)
              << " MHz, " << ChannelToBandwidthMhz(channelNumber) << " MHz wide.\n"
              << "Regulatory density " << REGULATORY_EIRP_DBM_PER_MHZ << " dBm/MHz gives "
              << std::fixed << std::setprecision(2)
              << GetRegulatoryTxPowerDbm(channelNumber) << " dBm in total.\n"
              << "Isotropic antennas at both ends, free space propagation, 100 octet frames.\n";

    std::cout << "\nWhat each data rate reaches, with a 128 symbol preamble at 62.4 MHz.\n\n"
              << std::left << std::setw(12) << "rate" << std::right << std::setw(12) << "Mb/s"
              << std::setw(11) << "gain dB" << std::setw(12) << "sens dBm" << std::setw(11)
              << "acq dBm" << std::setw(11) << "range m" << std::setw(11) << "frame us" << "\n"
              << std::string(80, '-') << "\n";

    config.prf = UwbPrf::PRF_64M;
    config.preambleSymbols = 128;
    for (const auto rate : {UwbDataRate::RATE_110K,
                            UwbDataRate::RATE_850K,
                            UwbDataRate::RATE_6M81,
                            UwbDataRate::RATE_27M24})
    {
        config.dataRate = rate;
        const double range = FindRange(rate, 128, UwbPrf::PRF_64M, frames, channelNumber);
        std::cout << std::left << std::setw(12) << UwbDataRateName(rate) << std::right
                  << std::fixed << std::setprecision(3) << std::setw(12)
                  << GetBitRate(rate) / 1e6 << std::setprecision(2) << std::setw(11)
                  << GetProcessingGainDb(config) << std::setw(12)
                  << errorModel->GetSensitivityDbm(config) << std::setw(11)
                  << errorModel->GetAcquisitionThresholdDbm(config) << std::setprecision(1)
                  << std::setw(11) << range << std::setw(11)
                  << GetFrameDuration(config, MAX_PSDU_OCTETS).GetMicroSeconds() << "\n";
    }

    std::cout << "\nSix dB of sensitivity is twice the range, so each step down the rates\n"
                 "roughly doubles the reach, and 27.24 Mb/s reaches a quarter of what\n"
                 "0.11 Mb/s does. None of these rows is limited by acquisition: at 62.4 MHz\n"
                 "the correlator has 127 chips per symbol to work with, so even a short\n"
                 "preamble finds the signal long before the payload becomes unreadable.\n";

    std::cout << "\nThe long range mode, 0.11 Mb/s, with the preamble varied. At 15.6 MHz the\n"
                 "codes are 31 chips rather than 127, so the correlator has four times less to\n"
                 "accumulate and the preamble becomes what runs out first.\n\n"
              << std::left << std::setw(8) << "PRF" << std::setw(11) << "preamble" << std::right
              << std::setw(12) << "acq dBm" << std::setw(12) << "sens dBm" << std::setw(11)
              << "limit" << std::setw(11) << "range m" << std::setw(11) << "frame us" << "\n"
              << std::string(76, '-') << "\n";

    config.dataRate = UwbDataRate::RATE_110K;
    for (const auto prf : {UwbPrf::PRF_16M, UwbPrf::PRF_64M})
    {
        config.prf = prf;
        for (const uint32_t preamble : {16u, 64u, 256u, 1024u})
        {
            config.preambleSymbols = preamble;
            const double acquisition = errorModel->GetAcquisitionThresholdDbm(config);
            const double sensitivity = errorModel->GetSensitivityDbm(config);
            const double range = FindRange(UwbDataRate::RATE_110K,
                                           preamble,
                                           prf,
                                           frames,
                                           channelNumber);
            std::cout << std::left << std::setw(8) << UwbPrfName(prf) << std::setw(11) << preamble
                      << std::right << std::fixed << std::setprecision(2) << std::setw(12)
                      << acquisition << std::setw(12) << sensitivity << std::setw(11)
                      << (acquisition > sensitivity ? "preamble" : "payload") << std::setprecision(1)
                      << std::setw(11) << range << std::setw(11)
                      << GetFrameDuration(config, MAX_PSDU_OCTETS).GetMicroSeconds() << "\n";
        }
    }

    std::cout << "\nWhere the limit column says preamble, the twelve dB the long range mode\n"
                 "won in the payload is being thrown away, because the receiver cannot find\n"
                 "the signal to begin with. Lengthening the preamble recovers it, and the\n"
                 "range roughly doubles for every four times the preamble, until the payload\n"
                 "takes over again. The price is air time: the frame column grows by about a\n"
                 "millisecond, which at 0.11 Mb/s is a tenth of the frame but at 6.81 Mb/s\n"
                 "would be several times the payload it precedes. That is the trade a UWB\n"
                 "system makes when it chooses a preamble length.\n\n";
    return 0;
}
