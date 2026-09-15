/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "uwb-interference-helper.h"

#include "ns3/log.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <vector>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("UwbInterferenceHelper");

namespace uwb
{

UwbInterferenceHelper::UwbInterferenceHelper()
{
}

void
UwbInterferenceHelper::AddSignal(Time start, Time stop, double powerW)
{
    NS_LOG_FUNCTION(this << start << stop << powerW);
    if (powerW <= 0.0 || stop <= start)
    {
        return;
    }
    m_signals.push_back(Signal{start, stop, powerW});
}

double
UwbInterferenceHelper::GetTotalPowerW(Time when) const
{
    double power = 0.0;
    for (const auto& signal : m_signals)
    {
        if (signal.start <= when && when < signal.stop)
        {
            power += signal.powerW;
        }
    }
    return power;
}

std::vector<Time>
UwbInterferenceHelper::GetBoundaries(Time start, Time stop) const
{
    std::set<Time> boundaries{start, stop};
    for (const auto& signal : m_signals)
    {
        if (signal.stop <= start || signal.start >= stop)
        {
            continue;
        }
        if (signal.start > start)
        {
            boundaries.insert(signal.start);
        }
        if (signal.stop < stop)
        {
            boundaries.insert(signal.stop);
        }
    }
    return {boundaries.begin(), boundaries.end()};
}

double
UwbInterferenceHelper::CalculateSuccessRate(Ptr<const UwbErrorModel> errorModel,
                                            double signalPowerW,
                                            double noiseW,
                                            Time start,
                                            Time stop,
                                            const UwbPhyConfig& config,
                                            uint64_t nbits) const
{
    NS_LOG_FUNCTION(this << signalPowerW << noiseW << start << stop << nbits);
    if (nbits == 0 || stop <= start)
    {
        return 1.0;
    }

    const auto points = GetBoundaries(start, stop);
    const double totalDuration = (stop - start).GetDouble();
    double logSuccess = 0.0;
    uint64_t assignedBits = 0;

    for (std::size_t i = 0; i + 1 < points.size(); ++i)
    {
        const Time chunkStart = points[i];
        const Time chunkStop = points[i + 1];
        const double fraction = (chunkStop - chunkStart).GetDouble() / totalDuration;
        // the bits of the last chunk absorb the rounding of the previous ones
        uint64_t chunkBits = (i + 2 == points.size())
                                 ? (nbits - assignedBits)
                                 : static_cast<uint64_t>(std::llround(fraction * nbits));
        chunkBits = std::min(chunkBits, nbits - assignedBits);
        assignedBits += chunkBits;
        if (chunkBits == 0)
        {
            continue;
        }

        // the signal being demodulated is recorded like every other, so it is removed here
        const Time middle = chunkStart + (chunkStop - chunkStart) / 2;
        const double interferenceW = std::max(GetTotalPowerW(middle) - signalPowerW, 0.0);
        const double sinr = signalPowerW / (noiseW + interferenceW);
        const double chunkSuccess = errorModel->GetChunkSuccessRate(sinr, config, chunkBits);
        if (chunkSuccess <= 0.0)
        {
            return 0.0;
        }
        logSuccess += std::log(chunkSuccess);
    }
    return std::exp(logSuccess);
}

double
UwbInterferenceHelper::GetAverageSinr(double signalPowerW,
                                      double noiseW,
                                      Time start,
                                      Time stop) const
{
    if (stop <= start || signalPowerW <= 0.0)
    {
        return 0.0;
    }

    const auto points = GetBoundaries(start, stop);
    const double totalDuration = (stop - start).GetDouble();
    double weighted = 0.0;

    for (std::size_t i = 0; i + 1 < points.size(); ++i)
    {
        const Time chunkStart = points[i];
        const Time chunkStop = points[i + 1];
        const Time middle = chunkStart + (chunkStop - chunkStart) / 2;
        const double interferenceW = std::max(GetTotalPowerW(middle) - signalPowerW, 0.0);
        const double sinr = signalPowerW / (noiseW + interferenceW);
        weighted += sinr * (chunkStop - chunkStart).GetDouble() / totalDuration;
    }
    return weighted;
}

void
UwbInterferenceHelper::Cleanup(Time before)
{
    m_signals.remove_if([before](const Signal& signal) { return signal.stop < before; });
}

void
UwbInterferenceHelper::Clear()
{
    m_signals.clear();
}

std::size_t
UwbInterferenceHelper::GetNSignals() const
{
    return m_signals.size();
}

} // namespace uwb
} // namespace ns3
