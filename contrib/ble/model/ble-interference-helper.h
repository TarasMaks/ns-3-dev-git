/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef BLE_INTERFERENCE_HELPER_H
#define BLE_INTERFERENCE_HELPER_H

#include "ble-constants.h"
#include "ble-error-model.h"

#include "ns3/nstime.h"
#include "ns3/ptr.h"

#include <list>

namespace ns3
{
namespace ble
{

/**
 * @ingroup ble
 * Track the signals that overlap a reception and turn them into a packet error probability.
 *
 * Every signal a PHY sees, whether it is the one being demodulated or not, is recorded with the
 * power it delivers inside the receiver bandwidth and with the interval it occupies. When a
 * reception ends, the interval is split at every point where the total interference changes and
 * the probability of receiving all the bits correctly is the product of the probabilities of the
 * pieces, each evaluated at the signal to interference and noise ratio of that piece.
 */
class BleInterferenceHelper
{
  public:
    BleInterferenceHelper();

    /**
     * Record a signal.
     *
     * @param start when the signal starts
     * @param stop when the signal ends
     * @param powerW the power it delivers inside the receiver bandwidth, in watt
     */
    void AddSignal(Time start, Time stop, double powerW);

    /**
     * The probability that every bit of a reception is correct.
     *
     * @param errorModel the error model of the PHY
     * @param signalPowerW the power of the signal being demodulated, in watt
     * @param noiseW the noise power in the receiver bandwidth, in watt
     * @param start when the reception starts
     * @param stop when the reception ends
     * @param mode the PHY mode of the reception
     * @param nbits the number of information bits of the reception
     * @return the probability that the packet is received without error
     */
    double CalculateSuccessRate(Ptr<const BleErrorModel> errorModel,
                                double signalPowerW,
                                double noiseW,
                                Time start,
                                Time stop,
                                BlePhyMode mode,
                                uint64_t nbits) const;

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
        double powerW; //!< the power inside the receiver bandwidth
    };

    std::list<Signal> m_signals; //!< the recorded signals
};

} // namespace ble
} // namespace ns3

#endif /* BLE_INTERFERENCE_HELPER_H */
