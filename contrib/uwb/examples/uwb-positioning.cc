/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Locating a tag in a room, two ways.
 *
 * A tag walks a circuit inside a square of four anchors. It is located twice over, by two
 * schemes that divide the work differently.
 *
 * Two-way ranging asks the tag to hold a conversation with each anchor in turn and measure the
 * distance to it, then puts the distances together by trilateration. The tag does the talking,
 * so the answer is available on the tag itself, which is what a device that wants to know where
 * it is needs.
 *
 * Time difference of arrival asks the tag for one frame and nothing else. The anchors timestamp
 * its arrival, the infrastructure compares the timestamps, and the tag never finds out. It costs
 * a quarter of the air time and none of the tag battery, and it puts the whole burden on the
 * anchors agreeing about time. The example shows what happens when they do and when they do not.
 */

#include "ns3/core-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/uwb-module.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <map>
#include <vector>

using namespace ns3;
using namespace ns3::uwb;

NS_LOG_COMPONENT_DEFINE("UwbPositioning");

namespace
{

/// The four corners of the room, which is the fewest anchors that can fix a position at all.
const std::vector<Vector> CORNERS{Vector(0, 0, 0),
                                  Vector(20, 0, 0),
                                  Vector(20, 20, 0),
                                  Vector(0, 20, 0)};

/// Two more anchors, halfway along the side walls, which is what an installation actually does.
const std::vector<Vector> EXTRA{Vector(0, 10, 0), Vector(20, 10, 0)};

std::vector<Vector> g_anchors; //!< the anchors of this run

std::vector<double> g_twrErrors;  //!< the error of each two-way fix, in metres
std::vector<double> g_tdoaErrors; //!< the error of each arrival difference fix, in metres
std::map<uint32_t, double> g_ranges; //!< the range to each anchor of the round in progress
std::map<Mac16Address, uint32_t> g_anchorIndex; //!< which anchor each short address is
uint32_t g_twrFailures = 0;          //!< rounds that could not be trilaterated
uint32_t g_tdoaFailures = 0;         //!< blinks that could not be solved
Ptr<MobilityModel> g_tagMobility;    //!< where the tag really is

/// @return the distance between two points, in the horizontal plane
double
Distance(Vector a, Vector b)
{
    return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
}

/**
 * A two-way exchange with one anchor finished.
 *
 * The tag hears about it either way: in a single-sided exchange it did the arithmetic itself,
 * and in a double-sided one the anchor did it and sent the answer back.
 *
 * @param result what was measured
 */
void
OnRange(const UwbRangingResult& result)
{
    if (!result.valid)
    {
        return;
    }
    auto anchor = g_anchorIndex.find(result.peer);
    if (anchor != g_anchorIndex.end())
    {
        g_ranges[anchor->second] = result.rangeMetres;
    }
}

/// A round of two-way exchanges is over, so put the distances together.
void
Trilaterated()
{
    if (g_ranges.size() < 3)
    {
        ++g_twrFailures;
        g_ranges.clear();
        return;
    }
    std::vector<Vector> anchors;
    std::vector<double> ranges;
    for (const auto& [index, range] : g_ranges)
    {
        anchors.push_back(g_anchors[index]);
        ranges.push_back(range);
    }
    g_ranges.clear();

    Vector estimate;
    if (!Trilaterate(anchors, ranges, false, estimate))
    {
        ++g_twrFailures;
        return;
    }
    g_twrErrors.push_back(Distance(estimate, g_tagMobility->GetPosition()));
}

/**
 * The infrastructure worked out where a tag was from the arrival differences.
 *
 * @param tag who was located
 * @param position where they were found
 * @param solved whether the geometry closed
 */
void
OnPosition(Mac16Address tag, Vector position, bool solved)
{
    if (!solved)
    {
        ++g_tdoaFailures;
        return;
    }
    g_tdoaErrors.push_back(Distance(position, g_tagMobility->GetPosition()));
}

/**
 * @param errors a set of position errors, in metres
 * @return the root mean square of them, in metres
 */
double
Rms(const std::vector<double>& errors)
{
    if (errors.empty())
    {
        return 0.0;
    }
    double sum = 0.0;
    for (const double error : errors)
    {
        sum += error * error / errors.size();
    }
    return std::sqrt(sum);
}

/**
 * @param errors a set of position errors, in metres
 * @param fraction the fraction of the errors to fall below the answer
 * @return the error that fraction of them fall below, in metres
 */
double
Percentile(std::vector<double> errors, double fraction)
{
    if (errors.empty())
    {
        return 0.0;
    }
    std::sort(errors.begin(), errors.end());
    const auto index = static_cast<std::size_t>(fraction * (errors.size() - 1));
    return errors[index];
}

} // namespace

int
main(int argc, char* argv[])
{
    uint32_t rounds = 60;
    uint32_t anchorCount = 6;
    double syncNs = 0.0;
    std::string method = "DS-TWR";
    double speed = 1.4;

    CommandLine cmd(__FILE__);
    cmd.AddValue("rounds", "How many times the tag is located by each scheme", rounds);
    cmd.AddValue("anchors", "How many anchors, four at the corners or six", anchorCount);
    cmd.AddValue("sync", "What the anchor clocks are left out by, in nanoseconds", syncNs);
    cmd.AddValue("method", "The two-way scheme to use, SS-TWR or DS-TWR", method);
    cmd.AddValue("speed", "How fast the tag walks, in metres per second", speed);
    cmd.Parse(argc, argv);

    const auto scheme = UwbRangingMethodFromString(method);

    g_anchors = CORNERS;
    for (std::size_t i = 0; i + CORNERS.size() < anchorCount && i < EXTRA.size(); ++i)
    {
        g_anchors.push_back(EXTRA[i]);
    }

    NodeContainer anchorNodes;
    anchorNodes.Create(g_anchors.size());
    NodeContainer tagNode;
    tagNode.Create(1);

    MobilityHelper anchorMobility;
    auto positions = CreateObject<ListPositionAllocator>();
    for (const auto& anchor : g_anchors)
    {
        positions->Add(anchor);
    }
    anchorMobility.SetPositionAllocator(positions);
    anchorMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    anchorMobility.Install(anchorNodes);

    // the tag walks a circuit inside the square, at the speed of someone crossing a warehouse
    MobilityHelper tagMobility;
    tagMobility.SetMobilityModel("ns3::WaypointMobilityModel");
    tagMobility.Install(tagNode);
    g_tagMobility = tagNode.Get(0)->GetObject<MobilityModel>();
    auto waypoints = DynamicCast<WaypointMobilityModel>(g_tagMobility);
    const std::vector<Vector> circuit{Vector(5, 5, 0),
                                      Vector(15, 5, 0),
                                      Vector(15, 15, 0),
                                      Vector(5, 15, 0),
                                      Vector(5, 5, 0)};
    Time when;
    for (std::size_t i = 0; i < circuit.size(); ++i)
    {
        if (i > 0)
        {
            when += Seconds(Distance(circuit[i], circuit[i - 1]) / speed);
        }
        waypoints->AddWaypoint(Waypoint(when, circuit[i]));
    }
    const Time circuitDuration = when;

    UwbHelper uwb;
    uwb.SetChannelNumber(5);
    uwb.SetDataRate(UwbDataRate::RATE_6M81);
    uwb.SetPreambleSymbols(128);
    uwb.SetChannel(UwbHelper::CreateChannel(5));

    auto anchorDevices = uwb.Install(anchorNodes);
    auto tagDevice = uwb.Install(tagNode.Get(0));
    NetDeviceContainer all;
    all.Add(anchorDevices);
    all.Add(tagDevice);
    uwb.AssignStreams(all, 1);

    auto engine = UwbHelper::CreateTdoaEngine(anchorDevices);
    engine->SetAttribute("SyncError", TimeValue(Seconds(syncNs * 1e-9)));
    engine->SetAttribute("CollectionWindow", TimeValue(MilliSeconds(2)));
    engine->AssignStreams(900);
    engine->SetPositionCallback(MakeCallback(&OnPosition));

    tagDevice->SetRangingResultCallback(MakeCallback(&OnRange));
    for (uint32_t i = 0; i < anchorDevices.GetN(); ++i)
    {
        g_anchorIndex[Mac16Address::ConvertFrom(anchorDevices.Get(i)->GetAddress())] = i;
    }

    // a round of two-way ranging is four exchanges, one with each anchor, spaced far enough
    // apart that they do not talk over one another; a round of arrival differences is one frame
    const Time roundSpacing = circuitDuration / rounds;
    const Time exchangeSpacing = MilliSeconds(5);
    for (uint32_t round = 0; round < rounds; ++round)
    {
        const Time start = roundSpacing * round + MilliSeconds(1);
        for (uint32_t i = 0; i < anchorDevices.GetN(); ++i)
        {
            const Mac16Address peer =
                Mac16Address::ConvertFrom(anchorDevices.Get(i)->GetAddress());
            Simulator::Schedule(start + exchangeSpacing * i, [tagDevice, peer, scheme]() {
                tagDevice->StartRanging(peer, scheme);
            });
        }
        Simulator::Schedule(start + exchangeSpacing * anchorDevices.GetN(), &Trilaterated);
        // the blink goes out in the middle of the round, so that both schemes see the tag in
        // about the same place, but in the gap between two exchanges rather than on top of
        // one: a half duplex radio that is already talking cannot blink
        Simulator::Schedule(start + exchangeSpacing * 2 + exchangeSpacing / 2,
                            [tagDevice]() { tagDevice->SendBlink(); });
    }

    Simulator::Stop(circuitDuration + Seconds(1));
    Simulator::Run();
    Simulator::Destroy();

    std::cout << "\nA tag walking a 40 m circuit at " << speed << " m/s inside "
              << g_anchors.size() << " anchors around a 20 m square.\n"
              << rounds << " rounds, channel 5, 6.81 Mb/s, anchor clocks out by " << syncNs
              << " ns.\n\n"
              << std::left << std::setw(34) << "scheme" << std::right << std::setw(10) << "fixes"
              << std::setw(10) << "failed" << std::setw(12) << "rms cm" << std::setw(12)
              << "median cm" << std::setw(12) << "p95 cm" << "\n"
              << std::string(90, '-') << "\n"
              << std::left << std::setw(34) << (method + " and trilateration") << std::right
              << std::setw(10) << g_twrErrors.size() << std::setw(10) << g_twrFailures
              << std::fixed << std::setprecision(1) << std::setw(12) << Rms(g_twrErrors) * 100
              << std::setw(12) << Percentile(g_twrErrors, 0.5) * 100 << std::setw(12)
              << Percentile(g_twrErrors, 0.95) * 100 << "\n"
              << std::left << std::setw(34) << "blink and arrival differences" << std::right
              << std::setw(10) << g_tdoaErrors.size() << std::setw(10) << g_tdoaFailures
              << std::setw(12) << Rms(g_tdoaErrors) * 100 << std::setw(12)
              << Percentile(g_tdoaErrors, 0.5) * 100 << std::setw(12)
              << Percentile(g_tdoaErrors, 0.95) * 100 << "\n\n"
              << "Two-way ranging spends one exchange per anchor and puts the answer on the\n"
                 "tag itself. Arrival differences spend one frame for the whole fix and put\n"
                 "the answer in the infrastructure, which is how a room holds hundreds of\n"
                 "tags, but they buy that with two things the two-way scheme does not need.\n\n"
                 "The first is time. Run this again with --sync=1 to see what one nanosecond\n"
                 "of disagreement between the anchor clocks costs.\n\n"
                 "The second is geometry. Run it with --anchors=4 and the median barely\n"
                 "moves while the 95th percentile blows up: four anchors leave exactly enough\n"
                 "equations to solve, and near the points where the tag is equidistant from\n"
                 "two pairs of them the system is close to singular, so the answer wanders by\n"
                 "metres even though the timestamps are as good as ever. The tag's circuit\n"
                 "crosses four such points. Two more anchors make the system overdetermined\n"
                 "and the blind spots go away, which is why installations never use the\n"
                 "minimum. Two-way ranging is far less sensitive to this, because a distance\n"
                 "is a stronger constraint than a difference of distances.\n\n";
    return 0;
}
