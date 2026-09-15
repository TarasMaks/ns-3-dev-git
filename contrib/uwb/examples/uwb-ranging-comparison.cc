/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Single-sided against double-sided two-way ranging.
 *
 * This is the experiment the module exists for. Two devices a known distance apart measure that
 * distance, first with a two-message exchange and then with a three-message one, using the same
 * radios, the same turnaround and the same crystals. The difference between the answers is the
 * whole argument for the extra frame.
 *
 * Run it with the defaults and the printed table shows the single-sided scheme out by metres and
 * the double-sided scheme out by a fraction of a centimetre. Sweep the crystal offset or the
 * turnaround and the single-sided error moves in proportion while the double-sided one does not.
 */

#include "ns3/core-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/uwb-module.h"

#include <cmath>
#include <iomanip>
#include <iostream>
#include <vector>

using namespace ns3;
using namespace ns3::uwb;

NS_LOG_COMPONENT_DEFINE("UwbRangingComparison");

namespace
{

/// Everything one run of the experiment measured.
struct Outcome
{
    std::vector<double> ranges; //!< every range that was measured
    uint32_t failed{0};         //!< exchanges that did not complete
    Time replyDelay{Time(0)};   //!< the turnaround the responder used

    /// @return the average of the ranges measured
    double Mean() const
    {
        double mean = 0.0;
        for (const double range : ranges)
        {
            mean += range / ranges.size();
        }
        return mean;
    }

    /// @return the standard deviation of the ranges measured
    double Sigma() const
    {
        const double mean = Mean();
        double variance = 0.0;
        for (const double range : ranges)
        {
            variance += (range - mean) * (range - mean) / ranges.size();
        }
        return std::sqrt(variance);
    }
};

Outcome g_outcome; //!< what the run in progress has measured so far

/// @param result what an exchange produced
void
OnResult(const UwbRangingResult& result)
{
    if (!result.valid)
    {
        ++g_outcome.failed;
        return;
    }
    // in a double-sided exchange the responder holds the answer and reports it back, so taking
    // only the device that did the arithmetic counts each exchange once
    if (!result.measuredHere)
    {
        return;
    }
    g_outcome.ranges.push_back(result.rangeMetres);
    g_outcome.replyDelay = result.replyDelay;
}

/**
 * Run one experiment.
 *
 * @param distance the separation of the two devices, in metres
 * @param method the ranging scheme
 * @param offsetPpm how far apart the two crystals are, in parts per million
 * @param reply the turnaround the responder is configured with
 * @param exchanges how many exchanges to run
 * @return what was measured
 */
Outcome
Measure(double distance, UwbRangingMethod method, double offsetPpm, Time reply, uint32_t exchanges)
{
    g_outcome = Outcome{};

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
    uwb.SetChannelNumber(5);
    uwb.SetDataRate(UwbDataRate::RATE_6M81);
    uwb.SetPreambleSymbols(128);
    uwb.SetMacAttribute("ResponseDelay", TimeValue(reply));
    uwb.SetChannel(UwbHelper::CreateChannel(5));

    // the two crystals are placed at either end of the spread, which is the worst case and the
    // one the standard permits
    auto offsets = CreateObject<UniformRandomVariable>();
    offsets->SetAttribute("Min", DoubleValue(offsetPpm / 2));
    offsets->SetAttribute("Max", DoubleValue(offsetPpm / 2));
    uwb.SetClockOffsetModel(offsets);
    auto initiator = uwb.Install(nodes.Get(0));

    offsets = CreateObject<UniformRandomVariable>();
    offsets->SetAttribute("Min", DoubleValue(-offsetPpm / 2));
    offsets->SetAttribute("Max", DoubleValue(-offsetPpm / 2));
    uwb.SetClockOffsetModel(offsets);
    auto responder = uwb.Install(nodes.Get(1));

    NetDeviceContainer devices;
    devices.Add(initiator);
    devices.Add(responder);
    uwb.AssignStreams(devices, 1);

    initiator->SetRangingResultCallback(MakeCallback(&OnResult));
    responder->SetRangingResultCallback(MakeCallback(&OnResult));

    const Mac16Address peer = Mac16Address::ConvertFrom(responder->GetAddress());
    const Time spacing = MilliSeconds(20);
    for (uint32_t i = 0; i < exchanges; ++i)
    {
        Simulator::Schedule(spacing * (i + 1), [initiator, peer, method]() {
            initiator->StartRanging(peer, method);
        });
    }

    Simulator::Stop(spacing * (exchanges + 3));
    Simulator::Run();
    Simulator::Destroy();
    return g_outcome;
}

/**
 * Print one row of the table.
 *
 * @param label what the row is
 * @param distance the true separation, in metres
 * @param outcome what was measured
 */
void
PrintRow(const std::string& label, double distance, const Outcome& outcome)
{
    if (outcome.ranges.empty())
    {
        std::cout << std::left << std::setw(26) << label << "  no exchange completed\n";
        return;
    }
    std::cout << std::left << std::setw(26) << label << std::right << std::fixed
              << std::setprecision(1) << std::setw(9) << outcome.replyDelay.GetMicroSeconds()
              << std::setprecision(3) << std::setw(12) << outcome.Mean() << std::setw(12)
              << (outcome.Mean() - distance) * 100.0 << std::setw(10) << outcome.Sigma() * 100.0
              << std::setw(9) << outcome.ranges.size() << "\n";
}

} // namespace

int
main(int argc, char* argv[])
{
    double distance = 10.0;
    uint32_t exchanges = 200;
    bool verbose = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("distance", "The true separation of the two devices, in metres", distance);
    cmd.AddValue("exchanges", "How many exchanges each configuration runs", exchanges);
    cmd.AddValue("verbose", "Turn on the logging of the MAC", verbose);
    cmd.Parse(argc, argv);

    if (verbose)
    {
        LogComponentEnable("UwbMac", LOG_LEVEL_INFO);
    }

    std::cout << "\nTwo devices " << distance << " m apart, channel 5 at 6.5 GHz, 6.81 Mb/s,\n"
              << "128 symbol preamble, measured " << exchanges << " times per row.\n\n"
              << std::left << std::setw(26) << "configuration" << std::right << std::setw(9)
              << "reply us" << std::setw(12) << "mean m" << std::setw(12) << "bias cm"
              << std::setw(10) << "sigma cm" << std::setw(9) << "runs" << "\n"
              << std::string(78, '-') << "\n";

    // the crystals are the variable; the turnaround is held at a typical 300 microseconds
    const Time reply = MicroSeconds(300);
    for (const double ppm : {0.0, 2.0, 10.0, 40.0})
    {
        std::ostringstream ss;
        ss << "SS-TWR, " << ppm << " ppm apart";
        PrintRow(ss.str(),
                 distance,
                 Measure(distance, UwbRangingMethod::SS_TWR, ppm, reply, exchanges));
    }
    std::cout << "\n";
    for (const double ppm : {0.0, 2.0, 10.0, 40.0})
    {
        std::ostringstream ss;
        ss << "DS-TWR, " << ppm << " ppm apart";
        PrintRow(ss.str(),
                 distance,
                 Measure(distance, UwbRangingMethod::DS_TWR, ppm, reply, exchanges));
    }

    std::cout << "\nThe same two crystals, 40 ppm apart, with the turnaround varied.\n\n"
              << std::left << std::setw(26) << "configuration" << std::right << std::setw(9)
              << "reply us" << std::setw(12) << "mean m" << std::setw(12) << "bias cm"
              << std::setw(10) << "sigma cm" << std::setw(9) << "runs" << "\n"
              << std::string(78, '-') << "\n";
    for (const uint32_t microseconds : {200, 400, 800, 1600})
    {
        const Time turnaround = MicroSeconds(microseconds);
        std::ostringstream ss;
        ss << "SS-TWR, " << microseconds << " us reply";
        PrintRow(ss.str(),
                 distance,
                 Measure(distance, UwbRangingMethod::SS_TWR, 40.0, turnaround, exchanges));
    }
    std::cout << "\n";
    for (const uint32_t microseconds : {200, 400, 800, 1600})
    {
        const Time turnaround = MicroSeconds(microseconds);
        std::ostringstream ss;
        ss << "DS-TWR, " << microseconds << " us reply";
        PrintRow(ss.str(),
                 distance,
                 Measure(distance, UwbRangingMethod::DS_TWR, 40.0, turnaround, exchanges));
    }

    std::cout << "\nThe single-sided bias is the crystal offset multiplied by half the\n"
                 "turnaround, and it is independent of the distance. The double-sided\n"
                 "estimator cancels it, leaving only the error the receiver makes\n"
                 "estimating when the signal arrived.\n\n";
    return 0;
}
