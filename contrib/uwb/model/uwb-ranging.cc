/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "uwb-ranging.h"

#include "uwb-clock-model.h"
#include "uwb-utils.h"

#include "ns3/log.h"

#include <cmath>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("UwbRanging");

namespace uwb
{

namespace
{

/// @param ticks a number of device time units, possibly negative
/// @return the duration it stands for, in seconds
double
TicksToSeconds(double ticks)
{
    return ticks / DTU_RATE_HZ;
}

} // namespace

double
SolveSsTwrSeconds(const UwbTwrTimestamps& stamps)
{
    const uint64_t round = UwbClockModel::TicksDifference(stamps.responseRx, stamps.pollTx);
    const uint64_t reply = UwbClockModel::TicksDifference(stamps.responseTx, stamps.pollRx);
    const double ticks = (static_cast<double>(round) - static_cast<double>(reply)) / 2.0;
    return TicksToSeconds(ticks);
}

double
SolveDsTwrSeconds(const UwbTwrTimestamps& stamps)
{
    const uint64_t round1 = UwbClockModel::TicksDifference(stamps.responseRx, stamps.pollTx);
    const uint64_t reply1 = UwbClockModel::TicksDifference(stamps.responseTx, stamps.pollRx);
    const uint64_t round2 = UwbClockModel::TicksDifference(stamps.finalRx, stamps.responseTx);
    const uint64_t reply2 = UwbClockModel::TicksDifference(stamps.finalTx, stamps.responseRx);

    // the products reach about 4e20, well past what a 64-bit integer holds, and the difference
    // between them is a tiny fraction of either, so they are formed exactly in 128 bits before
    // anything is rounded
    const auto numerator = static_cast<__int128>(round1) * round2 -
                           static_cast<__int128>(reply1) * reply2;
    const double denominator =
        static_cast<double>(round1) + round2 + static_cast<double>(reply1) + reply2;
    if (denominator <= 0.0)
    {
        return -1.0;
    }
    return TicksToSeconds(static_cast<double>(numerator) / denominator);
}

double
SolveSsTwrRange(const UwbTwrTimestamps& stamps)
{
    return SolveSsTwrSeconds(stamps) * SPEED_OF_LIGHT;
}

double
SolveDsTwrRange(const UwbTwrTimestamps& stamps)
{
    return SolveDsTwrSeconds(stamps) * SPEED_OF_LIGHT;
}

double
EstimateSsTwrClockErrorMetres(double offsetDifferencePpm, Time replyDelay)
{
    return offsetDifferencePpm * 1e-6 * replyDelay.GetSeconds() / 2.0 * SPEED_OF_LIGHT;
}

bool
SolveTdoa(const std::vector<Vector>& anchors,
          const std::vector<double>& arrivalOffsets,
          bool solveHeight,
          Vector& position)
{
    // the distance to the reference anchor is carried as an extra unknown, which is what makes
    // the hyperbolic system linear
    const std::size_t unknowns = (solveHeight ? 3 : 2) + 1;
    if (anchors.size() != arrivalOffsets.size() || anchors.size() < unknowns + 1)
    {
        return false;
    }

    const Vector& reference = anchors[0];
    const double referenceNorm =
        reference.x * reference.x + reference.y * reference.y + reference.z * reference.z;
    const std::size_t rows = anchors.size() - 1;

    std::vector<std::vector<double>> a(rows, std::vector<double>(unknowns, 0.0));
    std::vector<double> b(rows, 0.0);

    for (std::size_t i = 0; i < rows; ++i)
    {
        const Vector& anchor = anchors[i + 1];
        const double difference = (arrivalOffsets[i + 1] - arrivalOffsets[0]) * SPEED_OF_LIGHT;
        const double anchorNorm = anchor.x * anchor.x + anchor.y * anchor.y + anchor.z * anchor.z;

        a[i][0] = 2.0 * (anchor.x - reference.x);
        a[i][1] = 2.0 * (anchor.y - reference.y);
        if (solveHeight)
        {
            a[i][2] = 2.0 * (anchor.z - reference.z);
        }
        a[i][unknowns - 1] = 2.0 * difference;
        b[i] = anchorNorm - referenceNorm - difference * difference;
        if (!solveHeight)
        {
            // the tag is taken to be at the height of the reference anchor
            b[i] -= 2.0 * (anchor.z - reference.z) * reference.z;
        }
    }

    std::vector<double> solution;
    if (!SolveLeastSquares(a, b, solution))
    {
        return false;
    }

    // a negative distance to the reference anchor means the geometry did not close
    if (solution[unknowns - 1] < 0.0)
    {
        return false;
    }

    position.x = solution[0];
    position.y = solution[1];
    position.z = solveHeight ? solution[2] : reference.z;
    return true;
}

} // namespace uwb
} // namespace ns3
