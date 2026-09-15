/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * How many tags one UWB channel can carry.
 *
 * A UWB channel is shared by unslotted ALOHA, because a signal that sits below the noise floor
 * gives a carrier sense nothing to hear. What decides how many tags fit is therefore air time
 * and nothing else, and the two location schemes spend it very differently.
 *
 * A tag that ranges against four anchors holds four conversations, each of them three or four
 * frames long. A tag that blinks sends one frame and is done. The example puts a growing number
 * of tags in a room and measures how many of their attempts succeed, which is the number that
 * decides whether a system is usable.
 */

#include "ns3/core-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/uwb-module.h"

#include <iomanip>
#include <iostream>
#include <map>

using namespace ns3;
using namespace ns3::uwb;

NS_LOG_COMPONENT_DEFINE("UwbTagCapacity");

namespace
{

uint32_t g_attempted = 0; //!< location attempts started
uint32_t g_succeeded = 0; //!< attempts that produced a position

/// @param result what an exchange produced
void
OnRange(const UwbRangingResult& result)
{
    if (result.valid)
    {
        ++g_succeeded;
    }
}

/// @param tag who was located
/// @param position where they were found
/// @param solved whether the geometry closed
void
OnPosition(Mac16Address tag, Vector position, bool solved)
{
    if (solved)
    {
        ++g_succeeded;
    }
}

/// The anchors of the room.
const std::vector<Vector> ANCHORS{Vector(0, 0, 0),
                                  Vector(30, 0, 0),
                                  Vector(30, 30, 0),
                                  Vector(0, 30, 0),
                                  Vector(0, 15, 0),
                                  Vector(30, 15, 0)};

/**
 * Fill a room with tags and see how many of them get located.
 *
 * @param tags how many tags
 * @param twr true for two-way ranging against every anchor, false for blinking
 * @param ratePerSecond how often each tag tries to be located
 * @param duration how long the run lasts
 * @return the fraction of attempts that succeeded
 */
double
Run(uint32_t tags, bool twr, double ratePerSecond, Time duration)
{
    g_attempted = 0;
    g_succeeded = 0;

    NodeContainer anchorNodes;
    anchorNodes.Create(ANCHORS.size());
    NodeContainer tagNodes;
    tagNodes.Create(tags);

    MobilityHelper mobility;
    auto anchorPositions = CreateObject<ListPositionAllocator>();
    for (const auto& anchor : ANCHORS)
    {
        anchorPositions->Add(anchor);
    }
    mobility.SetPositionAllocator(anchorPositions);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(anchorNodes);

    mobility.SetPositionAllocator("ns3::RandomBoxPositionAllocator",
                                  "X",
                                  StringValue("ns3::UniformRandomVariable[Min=2|Max=28]"),
                                  "Y",
                                  StringValue("ns3::UniformRandomVariable[Min=2|Max=28]"),
                                  "Z",
                                  StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    mobility.Install(tagNodes);

    UwbHelper uwb;
    uwb.SetChannelNumber(5);
    uwb.SetDataRate(UwbDataRate::RATE_6M81);
    uwb.SetPreambleSymbols(128);
    uwb.SetChannel(UwbHelper::CreateChannel(5));

    auto anchorDevices = uwb.Install(anchorNodes);
    auto tagDevices = uwb.Install(tagNodes);
    NetDeviceContainer all;
    all.Add(anchorDevices);
    all.Add(tagDevices);
    uwb.AssignStreams(all, 1);

    auto engine = UwbHelper::CreateTdoaEngine(anchorDevices);
    engine->AssignStreams(9000);
    engine->SetPositionCallback(MakeCallback(&OnPosition));

    for (uint32_t i = 0; i < tagDevices.GetN(); ++i)
    {
        DynamicCast<UwbNetDevice>(tagDevices.Get(i))
            ->SetRangingResultCallback(MakeCallback(&OnRange));
    }

    // the tags are not synchronised with one another, which is the point: they collide
    auto jitter = CreateObject<UniformRandomVariable>();
    jitter->SetStream(12345);

    const Time period = Seconds(1.0 / ratePerSecond);
    const uint32_t attempts = static_cast<uint32_t>(duration.GetSeconds() * ratePerSecond);
    for (uint32_t i = 0; i < tagDevices.GetN(); ++i)
    {
        auto tag = DynamicCast<UwbNetDevice>(tagDevices.Get(i));
        const Time offset = period * jitter->GetValue(0.0, 1.0);
        for (uint32_t attempt = 0; attempt < attempts; ++attempt)
        {
            const Time at = offset + period * attempt + MilliSeconds(10);
            if (!twr)
            {
                ++g_attempted;
                Simulator::Schedule(at, [tag]() { tag->SendBlink(); });
                continue;
            }
            // one exchange per anchor, spread across the period so that a single tag does not
            // collide with itself
            for (uint32_t a = 0; a < anchorDevices.GetN(); ++a)
            {
                ++g_attempted;
                const Mac16Address peer =
                    Mac16Address::ConvertFrom(anchorDevices.Get(a)->GetAddress());
                Simulator::Schedule(at + MilliSeconds(3) * a, [tag, peer]() {
                    tag->StartRanging(peer, UwbRangingMethod::DS_TWR);
                });
            }
        }
    }

    Simulator::Stop(duration + Seconds(1));
    Simulator::Run();
    Simulator::Destroy();
    return (g_attempted == 0) ? 0.0 : static_cast<double>(g_succeeded) / g_attempted;
}

} // namespace

int
main(int argc, char* argv[])
{
    double ratePerSecond = 10.0;
    double seconds = 2.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("rate", "How often each tag asks to be located, per second", ratePerSecond);
    cmd.AddValue("seconds", "How long each run lasts", seconds);
    cmd.Parse(argc, argv);

    const Time duration = Seconds(seconds);

    UwbPhyConfig config;
    config.channel = 5;
    config.dataRate = UwbDataRate::RATE_6M81;
    config.prf = UwbPrf::PRF_64M;
    config.preambleSymbols = 128;
    const Time pollFrame = GetFrameDuration(config, 14);

    std::cout << "\nA 30 m room with " << ANCHORS.size()
              << " anchors, channel 5, 6.81 Mb/s, 128 symbol preamble.\n"
              << "Each tag asks to be located " << ratePerSecond << " times a second.\n"
              << "A ranging frame lasts " << pollFrame.GetMicroSeconds()
              << " us on the air, so the channel holds about "
              << static_cast<uint32_t>(1.0 / pollFrame.GetSeconds())
              << " frames a second before it is full.\n\n"
              << std::right << std::setw(8) << "tags" << std::setw(16) << "TWR frames/s"
              << std::setw(14) << "TWR success" << std::setw(17) << "blink frames/s"
              << std::setw(16) << "blink success" << "\n"
              << std::string(71, '-') << "\n";

    for (const uint32_t tags : {1u, 5u, 10u, 25u, 50u, 100u})
    {
        // a two-way fix costs four frames per anchor; a blink costs one frame in total
        const double twrFrames = tags * ratePerSecond * ANCHORS.size() * 4;
        const double blinkFrames = tags * ratePerSecond;
        const double twrSuccess = Run(tags, true, ratePerSecond, duration);
        const double blinkSuccess = Run(tags, false, ratePerSecond, duration);
        std::cout << std::right << std::setw(8) << tags << std::fixed << std::setprecision(0)
                  << std::setw(16) << twrFrames << std::setprecision(1) << std::setw(13)
                  << twrSuccess * 100 << "%" << std::setprecision(0) << std::setw(16)
                  << blinkFrames << std::setprecision(1) << std::setw(15) << blinkSuccess * 100
                  << "%" << "\n";
    }

    std::cout << "\nThe two schemes buy the same thing at very different prices. A two-way fix\n"
                 "costs four frames for each anchor it talks to and the channel fills up long\n"
                 "before the tags do; a blink costs one frame however many anchors are\n"
                 "listening, so the same channel carries an order of magnitude more tags. What\n"
                 "the blink gives up is that the tag never learns where it is, and that the\n"
                 "anchors have to agree about time to within a nanosecond.\n\n";
    return 0;
}
