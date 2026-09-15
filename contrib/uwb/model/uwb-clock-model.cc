/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "uwb-clock-model.h"

#include "ns3/double.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"

#include <cmath>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("UwbClockModel");

namespace uwb
{

namespace
{

/**
 * The device time counter runs at 63.8976 GHz, so one femtosecond is 638976 / 10^10 ticks.
 * Keeping the conversion as that exact ratio, evaluated in 128-bit arithmetic, means a
 * timestamp never drifts from the tick grid by rounding, however long the simulation runs.
 */
constexpr int64_t TICKS_PER_FEMTOSECOND_NUMERATOR = 638976;
/// Denominator of the same ratio.
constexpr int64_t TICKS_PER_FEMTOSECOND_DENOMINATOR = 10000000000LL;
/// Mask that reduces a counter reading to the width of the hardware counter.
constexpr uint64_t COUNTER_MASK = DTU_COUNTER_MODULUS - 1;

} // namespace

NS_OBJECT_ENSURE_REGISTERED(UwbClockModel);

TypeId
UwbClockModel::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::uwb::UwbClockModel")
            .SetParent<Object>()
            .SetGroupName("Uwb")
            .AddConstructor<UwbClockModel>()
            .AddAttribute("FrequencyOffset",
                          "Frequency error of the crystal, in parts per million, positive when "
                          "it runs fast. Crystals sold for UWB are specified at 2 ppm at the "
                          "careful end and 20 ppm at the cheap end; IEEE Std 802.15.4 requires "
                          "at most 20 ppm.",
                          DoubleValue(0.0),
                          MakeDoubleAccessor(&UwbClockModel::SetFrequencyOffsetPpm,
                                             &UwbClockModel::GetFrequencyOffsetPpm),
                          MakeDoubleChecker<double>(-100.0, 100.0))
            .AddAttribute("Drift",
                          "Rate at which the frequency error itself changes, in parts per "
                          "million per second, standing in for a crystal whose temperature is "
                          "moving. Zero, the default, is a crystal that has settled.",
                          DoubleValue(0.0),
                          MakeDoubleAccessor(&UwbClockModel::m_driftPpmPerSecond),
                          MakeDoubleChecker<double>(-1.0, 1.0))
            .AddAttribute("InitialTicks",
                          "Reading of the device counter at the start of the simulation. Two "
                          "devices that start from the same value would agree on absolute time, "
                          "which no two real devices do.",
                          UintegerValue(0),
                          MakeUintegerAccessor(&UwbClockModel::m_initialTicks),
                          MakeUintegerChecker<uint64_t>(0, DTU_COUNTER_MODULUS - 1));
    return tid;
}

UwbClockModel::UwbClockModel()
    : m_frequencyOffsetPpm(0.0),
      m_driftPpmPerSecond(0.0),
      m_initialTicks(0)
{
    NS_LOG_FUNCTION(this);
}

UwbClockModel::~UwbClockModel()
{
    NS_LOG_FUNCTION(this);
}

Time
UwbClockModel::TicksToTime(uint64_t ticks)
{
    const auto femtoseconds = static_cast<int64_t>(
        (static_cast<__int128>(ticks) * TICKS_PER_FEMTOSECOND_DENOMINATOR +
         TICKS_PER_FEMTOSECOND_NUMERATOR / 2) /
        TICKS_PER_FEMTOSECOND_NUMERATOR);
    return FemtoSeconds(femtoseconds);
}

uint64_t
UwbClockModel::TimeToTicks(Time duration)
{
    const int64_t femtoseconds = duration.ToInteger(Time::FS);
    if (femtoseconds <= 0)
    {
        return 0;
    }
    return static_cast<uint64_t>(
        (static_cast<__int128>(femtoseconds) * TICKS_PER_FEMTOSECOND_NUMERATOR +
         TICKS_PER_FEMTOSECOND_DENOMINATOR / 2) /
        TICKS_PER_FEMTOSECOND_DENOMINATOR);
}

uint64_t
UwbClockModel::TicksDifference(uint64_t later, uint64_t earlier)
{
    return (later - earlier) & COUNTER_MASK;
}

double
UwbClockModel::GetInstantaneousOffsetPpm(Time global) const
{
    return m_frequencyOffsetPpm + m_driftPpmPerSecond * global.GetSeconds();
}

Time
UwbClockModel::GetLocalTime(Time global) const
{
    // the integral of a frequency error that grows linearly leaves the drift acting at half
    // its instantaneous value over the interval
    const double seconds = global.GetSeconds();
    const double meanOffsetPpm = m_frequencyOffsetPpm + m_driftPpmPerSecond * seconds / 2.0;
    const int64_t femtoseconds = global.ToInteger(Time::FS);
    const auto correction = static_cast<int64_t>(
        std::llround(static_cast<double>(femtoseconds) * meanOffsetPpm * 1e-6));
    return FemtoSeconds(femtoseconds + correction);
}

Time
UwbClockModel::LocalToGlobal(Time local) const
{
    // invert the quadratic by iteration; with a drift of at most 1 ppm per second the first
    // correction already lands well inside a femtosecond
    double seconds = local.GetSeconds();
    for (int i = 0; i < 4; ++i)
    {
        const double meanOffsetPpm = m_frequencyOffsetPpm + m_driftPpmPerSecond * seconds / 2.0;
        seconds = local.GetSeconds() / (1.0 + meanOffsetPpm * 1e-6);
    }
    return Seconds(seconds);
}

uint64_t
UwbClockModel::GetLocalTicks(Time global) const
{
    return (TimeToTicks(GetLocalTime(global)) + m_initialTicks) & COUNTER_MASK;
}

Time
UwbClockModel::ResolveTimestamp(uint64_t ticks, Time reference) const
{
    const uint64_t expected = TimeToTicks(GetLocalTime(reference)) + m_initialTicks;
    const auto modulus = static_cast<int64_t>(DTU_COUNTER_MODULUS);
    const auto epoch = static_cast<int64_t>(expected / DTU_COUNTER_MODULUS);

    int64_t full = epoch * modulus + static_cast<int64_t>(ticks & COUNTER_MASK);
    if (full - static_cast<int64_t>(expected) > modulus / 2)
    {
        full -= modulus;
    }
    else if (static_cast<int64_t>(expected) - full > modulus / 2)
    {
        full += modulus;
    }

    if (full <= static_cast<int64_t>(m_initialTicks))
    {
        return Time(0);
    }
    return LocalToGlobal(TicksToTime(static_cast<uint64_t>(full) - m_initialTicks));
}

uint64_t
UwbClockModel::GetLocalTicksNow() const
{
    return GetLocalTicks(Simulator::Now());
}

void
UwbClockModel::SetFrequencyOffsetPpm(double ppm)
{
    NS_LOG_FUNCTION(this << ppm);
    m_frequencyOffsetPpm = ppm;
}

double
UwbClockModel::GetFrequencyOffsetPpm() const
{
    return m_frequencyOffsetPpm;
}

void
UwbClockModel::RandomiseFrequencyOffset(Ptr<RandomVariableStream> stream)
{
    NS_LOG_FUNCTION(this << stream);
    NS_ASSERT_MSG(stream, "A random variable is needed to draw a frequency offset");
    SetFrequencyOffsetPpm(stream->GetValue());
}

void
UwbClockModel::RandomiseInitialTicks(Ptr<RandomVariableStream> stream)
{
    NS_LOG_FUNCTION(this << stream);
    NS_ASSERT_MSG(stream, "A random variable is needed to draw a counter start");
    m_initialTicks = static_cast<uint64_t>(std::llround(stream->GetValue())) & COUNTER_MASK;
}

} // namespace uwb
} // namespace ns3
