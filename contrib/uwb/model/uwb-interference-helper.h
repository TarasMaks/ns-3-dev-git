/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef UWB_INTERFERENCE_HELPER_H
#define UWB_INTERFERENCE_HELPER_H

#include "uwb-error-model.h"

#include "ns3/nstime.h"
#include "ns3/ptr.h"

#include <list>

namespace ns3
{
namespace uwb
{

/**
 * @ingroup uwb
 * Track the signals that overlap a reception and turn them into a packet error probability.
 *
 * Every signal a PHY sees, whether it is the one being demodulated or not, is recorded with the
 * power it delivers inside the channel and with the interval it occupies. When a reception
 * ends, the interval is split at every point where the total interference changes and the
 * probability of receiving all the bits correctly is the product of the probabilities of the
 * pieces, each evaluated at the signal to interference and noise ratio of that piece.
 *
 * The power a signal is recorded with is the power that survives the receiver, so the PHY has
 * already applied the rejection that a mismatched preamble code earns. A narrowband
 * transmitter sharing the band, a Wi-Fi radio for instance, is recorded with the whole of the
 * power that falls inside the channel; the processing gain that the error model applies is what
 * makes UWB tolerate it.
 */
class UwbInterferenceHelper
{
  public:
    UwbInterferenceHelper();

    /**
     * Record a signal.
     *
     * @param start when the signal starts
     * @param stop when the signal ends
     * @param powerW the power it delivers inside the channel, in watt, after any rejection the
     *               receiver applies to it
     */
    void AddSignal(Time start, Time stop, double powerW);

    /**
     * The probability that every bit of a reception is correct.
     *
     * @param errorModel the error model of the PHY
     * @param signalPowerW the power of the signal being demodulated, in watt
     * @param noiseW the noise power in the channel, in watt
     * @param start when the payload starts
     * @param stop when the payload ends
     * @param config the link configuration of the reception
     * @param nbits the number of information bits of the reception
     * @return the probability that the packet is received without error
     */
    double CalculateSuccessRate(Ptr<const UwbErrorModel> errorModel,
                                double signalPowerW,
                                double noiseW,
                                Time start,
                                Time stop,
                                const UwbPhyConfig& config,
                                uint64_t nbits) const;

    /**
     * The signal to interference and noise ratio averaged over an interval, which is the figure
     * a correlator that integrates over the whole preamble sees.
     *
     * @param signalPowerW the power of the signal being acquired, in watt
     * @param noiseW the noise power in the channel, in watt
     * @param start the start of the interval
     * @param stop the end of the interval
     * @return the average signal to interference and noise ratio, as a linear ratio
     */
    double GetAverageSinr(double signalPowerW, double noiseW, Time start, Time stop) const;

    /**
     * The total power of every signal present at an instant.
     *
     * @param when the instant
     * @return the power, in watt
     */
    double GetTotalPowerW(Time when) const;

    /**
     * Forget the signals that ended before an instant.
     *
     * @param before the instant
     */
    void Cleanup(Time before);

    /// Forget every recorded signal.
    void Clear();

    /// @return the number of recorded signals
    std::size_t GetNSignals() const;

  private:
    /// One recorded signal.
    struct Signal
    {
        Time start;    //!< when the signal starts
        Time stop;     //!< when the signal ends
        double powerW; //!< the power inside the channel, after rejection
    };

    /**
     * The instants inside an interval at which the set of overlapping signals changes, with the
     * bounds of the interval included.
     *
     * @param start the start of the interval
     * @param stop the end of the interval
     * @return the instants, in order
     */
    std::vector<Time> GetBoundaries(Time start, Time stop) const;

    std::list<Signal> m_signals; //!< the recorded signals
};

} // namespace uwb
} // namespace ns3

#endif /* UWB_INTERFERENCE_HELPER_H */
