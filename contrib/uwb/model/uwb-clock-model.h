/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef UWB_CLOCK_MODEL_H
#define UWB_CLOCK_MODEL_H

#include "uwb-constants.h"

#include "ns3/nstime.h"
#include "ns3/object.h"
#include "ns3/random-variable-stream.h"

namespace ns3
{
namespace uwb
{

/**
 * @ingroup uwb
 * The crystal of one UWB device: the counter that timestamps every transmission and arrival.
 *
 * This class is the reason the module exists in the form it does. A UWB device does not know
 * what time it is; it knows what its own counter says, and the counter runs at 63.8976 GHz
 * plus or minus whatever the crystal is off by. A 20 ppm crystal, which is an ordinary part,
 * gains or loses 20 microseconds every second. Over the 300 microseconds a responder typically
 * takes to turn a ranging frame around, two devices 40 ppm apart accumulate 12 ns of
 * disagreement, which is 3.6 metres of apparent range. That single number is why single-sided
 * two-way ranging is nearly useless without correction and why the double-sided scheme exists,
 * and a model that let every device read a perfect global clock could not show it.
 *
 * The counter is 40 bits wide, like the counter of real hardware, so it wraps about every 17.2
 * seconds. Differences are therefore taken modulo the counter width, which is what ranging
 * firmware has to do as well; TicksDifference does it correctly.
 *
 * The local time of a device is
 *
 *     t_local = offset + t * (1 + (e + d * t / 2) * 1e-6),
 *
 * where e is the constant frequency error of the crystal in parts per million and d is a slow
 * drift, in parts per million per second, that stands in for the crystal warming up. The
 * quadratic term is the integral of a frequency error that grows linearly with time.
 */
class UwbClockModel : public Object
{
  public:
    /// @return the TypeId
    static TypeId GetTypeId();

    UwbClockModel();
    ~UwbClockModel() override;

    /**
     * The reading of the device counter at an instant of simulated time.
     *
     * @param global the instant
     * @return the counter reading, in device time units, already reduced modulo the counter
     *         width
     */
    uint64_t GetLocalTicks(Time global) const;

    /// @return the reading of the device counter now
    uint64_t GetLocalTicksNow() const;

    /**
     * The local time of a device, as a Time rather than a counter reading, without the wrap.
     *
     * @param global the instant of simulated time
     * @return the local time
     */
    Time GetLocalTime(Time global) const;

    /**
     * The instant of simulated time at which the device counter reaches a local time.
     *
     * @param local the local time
     * @return the instant of simulated time
     */
    Time LocalToGlobal(Time local) const;

    /**
     * The difference between two counter readings, taken the way ranging firmware has to take
     * it, so that a wrap of the 40-bit counter between them does not corrupt the result.
     *
     * @param later the later reading
     * @param earlier the earlier reading
     * @return the number of device time units between them
     */
    static uint64_t TicksDifference(uint64_t later, uint64_t earlier);

    /**
     * Convert a number of device time units into a duration.
     *
     * @param ticks the number of device time units
     * @return the duration
     */
    static Time TicksToTime(uint64_t ticks);

    /**
     * Convert a duration into a number of device time units, rounded to the nearest one.
     *
     * @param duration the duration
     * @return the number of device time units
     */
    static uint64_t TimeToTicks(Time duration);

    /**
     * Set the frequency error of the crystal.
     *
     * @param ppm the error, in parts per million, positive when the crystal runs fast
     */
    void SetFrequencyOffsetPpm(double ppm);

    /// @return the frequency error of the crystal, in parts per million
    double GetFrequencyOffsetPpm() const;

    /**
     * The frequency error of the crystal at an instant, including the slow drift.
     *
     * @param global the instant
     * @return the error, in parts per million
     */
    double GetInstantaneousOffsetPpm(Time global) const;

    /**
     * Draw a frequency error for this crystal from a random variable, which is how a population
     * of devices with the same specification is built.
     *
     * @param stream the random variable, in parts per million
     */
    void RandomiseFrequencyOffset(Ptr<RandomVariableStream> stream);

    /**
     * Draw the starting value of the counter, so that two devices do not happen to agree.
     *
     * @param stream the random variable, in device time units
     */
    void RandomiseInitialTicks(Ptr<RandomVariableStream> stream);

  private:
    double m_frequencyOffsetPpm; //!< constant frequency error of the crystal
    double m_driftPpmPerSecond;  //!< rate at which the frequency error itself changes
    uint64_t m_initialTicks;     //!< reading of the counter at the start of the simulation
};

} // namespace uwb
} // namespace ns3

#endif /* UWB_CLOCK_MODEL_H */
