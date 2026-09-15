/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef UWB_RANGING_H
#define UWB_RANGING_H

#include "uwb-constants.h"

#include "ns3/nstime.h"
#include "ns3/vector.h"

#include <cstdint>
#include <vector>

namespace ns3
{
namespace uwb
{

/**
 * @ingroup uwb
 * The six timestamps a full two-way exchange produces, each a reading of the counter of
 * whichever device took it.
 *
 * The initiator reads the counter three times, at pollTx, responseRx and finalTx; the responder
 * reads it three times, at pollRx, responseTx and finalRx. No two of these readings come from
 * the same clock unless they were taken by the same device, which is the whole difficulty of
 * ranging and the reason the two schemes differ.
 */
struct UwbTwrTimestamps
{
    uint64_t pollTx{0};     //!< initiator, sends the poll
    uint64_t pollRx{0};     //!< responder, receives the poll
    uint64_t responseTx{0}; //!< responder, sends the response
    uint64_t responseRx{0}; //!< initiator, receives the response
    uint64_t finalTx{0};    //!< initiator, sends the final message
    uint64_t finalRx{0};    //!< responder, receives the final message
};

/**
 * @ingroup uwb
 * The time of flight a single-sided two-way exchange measures.
 *
 * Two messages: the initiator polls, the responder answers, and the round trip minus the time
 * the responder spent thinking, halved, is the one-way flight,
 *
 *     tof = (Tround - Treply) / 2.
 *
 * The two durations are measured on different crystals. If they differ by e parts per million,
 * the estimate is wrong by e * Treply / 2, which for two ordinary 20 ppm crystals and a reply
 * delay of 300 microseconds is 6 ns, or 1.8 metres. That error does not shrink with distance
 * and it does not average away, because the crystals do not change between measurements. It is
 * why single-sided ranging is used only where the clock offset has been measured separately,
 * from the carrier frequency offset of the received signal, and corrected for.
 *
 * @param stamps the four timestamps of the exchange; the final pair is not used
 * @return the time of flight, in seconds, which is negative when the measurement is nonsense
 */
double SolveSsTwrSeconds(const UwbTwrTimestamps& stamps);

/**
 * @ingroup uwb
 * The time of flight a double-sided two-way exchange measures.
 *
 * Three messages, so that each device measures one round trip and one reply delay, and the
 * asymmetric estimator of IEEE Std 802.15.4z combines them,
 *
 *     tof = (Tround1 * Tround2 - Treply1 * Treply2)
 *           / (Tround1 + Tround2 + Treply1 + Treply2).
 *
 * The clock errors of the two devices appear in the numerator and the denominator alike and
 * cancel to first order. What is left is the true flight multiplied by half the sum of the two
 * offsets, which for 20 ppm crystals over ten metres is a fifth of a millimetre. This is why an
 * extra message is worth sending, and the test suite measures both schemes side by side to show
 * the three orders of magnitude between them.
 *
 * @param stamps the six timestamps of the exchange
 * @return the time of flight, in seconds, which is negative when the measurement is nonsense
 */
double SolveDsTwrSeconds(const UwbTwrTimestamps& stamps);

/**
 * @ingroup uwb
 * The distance a single-sided two-way exchange measures.
 *
 * @param stamps the timestamps of the exchange
 * @return the distance, in metres
 */
double SolveSsTwrRange(const UwbTwrTimestamps& stamps);

/**
 * @ingroup uwb
 * The distance a double-sided two-way exchange measures.
 *
 * @param stamps the timestamps of the exchange
 * @return the distance, in metres
 */
double SolveDsTwrRange(const UwbTwrTimestamps& stamps);

/**
 * @ingroup uwb
 * The range error a single-sided exchange makes because the two crystals disagree.
 *
 * @param offsetDifferencePpm the frequency offset of the initiator minus that of the responder
 * @param replyDelay how long the responder takes to turn the poll around
 * @return the error, in metres, signed
 */
double EstimateSsTwrClockErrorMetres(double offsetDifferencePpm, Time replyDelay);

/**
 * @ingroup uwb
 * Estimate a position from the differences between the times a single transmission reached
 * several anchors.
 *
 * Time difference of arrival needs no reply from the tag: the tag transmits once and the
 * infrastructure works out where it was. What it needs instead is anchors that agree on time,
 * because a nanosecond of disagreement between two anchors moves the answer by thirty
 * centimetres. Each pair of anchors places the tag on a hyperbola; the intersection is the
 * position.
 *
 * The system is linearised the usual way, by treating the distance to the reference anchor as a
 * fourth unknown, which needs one more anchor than trilateration does: four in a plane, five to
 * resolve height.
 *
 * @param anchors the positions of the anchors, the first of which is the reference
 * @param arrivalOffsets the arrival time at each anchor minus the arrival time at the reference
 *                       anchor, in seconds; the first entry is zero by construction
 * @param solveHeight whether the vertical coordinate is estimated as well
 * @param position filled with the estimated position
 * @return true if a position could be computed
 */
bool SolveTdoa(const std::vector<Vector>& anchors,
               const std::vector<double>& arrivalOffsets,
               bool solveHeight,
               Vector& position);

} // namespace uwb
} // namespace ns3

#endif /* UWB_RANGING_H */
