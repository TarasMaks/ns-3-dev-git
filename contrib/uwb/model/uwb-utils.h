/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef UWB_UTILS_H
#define UWB_UTILS_H

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
 * The configuration of one HRP UWB link: channel, data rate, pulse repetition frequency and
 * preamble length. Both ends of a transmission must agree on all of it.
 */
struct UwbPhyConfig
{
    uint8_t channel{5};                          //!< channel index (1 to 15)
    UwbDataRate dataRate{UwbDataRate::RATE_6M81}; //!< data rate of the payload
    UwbPrf prf{UwbPrf::PRF_64M};                 //!< mean pulse repetition frequency
    uint32_t preambleSymbols{128};               //!< length of the synchronisation preamble

    /// @return the number of symbols of the start of frame delimiter
    uint32_t GetSfdSymbols() const;

    /// @return a printable summary
    std::string ToString() const;
};

/**
 * @ingroup uwb
 * The number of bits a payload occupies once the Reed-Solomon outer code of the physical layer
 * has been applied (IEEE Std 802.15.4, Clause 15.3.3).
 *
 * The payload is split into blocks of 330 information bits, and each block, including a
 * shortened last one, carries 48 parity bits.
 *
 * @param octets the payload length, in octets
 * @return the number of coded bits
 */
uint32_t GetReedSolomonCodedBits(uint32_t octets);

/**
 * @ingroup uwb
 * The duration of the synchronisation header, that is the preamble and the start of frame
 * delimiter.
 *
 * @param config the link configuration
 * @return the duration
 */
Time GetShrDuration(const UwbPhyConfig& config);

/**
 * @ingroup uwb
 * The duration of the physical layer header.
 *
 * @param config the link configuration
 * @return the duration
 */
Time GetPhrDuration(const UwbPhyConfig& config);

/**
 * @ingroup uwb
 * The duration of the payload of a frame, including its frame check sequence.
 *
 * @param config the link configuration
 * @param psduOctets the payload length, in octets, including the frame check sequence
 * @return the duration
 */
Time GetPsduDuration(const UwbPhyConfig& config, uint32_t psduOctets);

/**
 * @ingroup uwb
 * The duration of a whole frame on the air.
 *
 * @param config the link configuration
 * @param psduOctets the payload length, in octets, including the frame check sequence
 * @return the duration
 */
Time GetFrameDuration(const UwbPhyConfig& config, uint32_t psduOctets);

/**
 * @ingroup uwb
 * The processing gain of a link, that is the ratio between the occupied bandwidth and the
 * information rate. It is what lets a UWB receiver work far below the noise floor of its own
 * channel.
 *
 * @param config the link configuration
 * @return the processing gain, in dB
 */
double GetProcessingGainDb(const UwbPhyConfig& config);

/**
 * @ingroup uwb
 * The total transmit power the regulatory density limit allows over a channel.
 *
 * @param channel the channel index
 * @param densityDbmPerMhz the power density, in dBm per megahertz
 * @return the total power, in dBm
 */
double GetRegulatoryTxPowerDbm(uint8_t channel,
                               double densityDbmPerMhz = REGULATORY_EIRP_DBM_PER_MHZ);

/**
 * @ingroup uwb
 * Raise the resolution of the simulator clock to femtoseconds, which this module needs.
 *
 * The default resolution of ns-3 is one nanosecond, which is thirty centimetres of propagation:
 * every ranging measurement would collapse onto that grid and the module would report nothing
 * but quantisation noise. One femtosecond is 0.3 micrometres, comfortably below the 15.65 ps
 * (4.69 mm) timestamp grid of the radios themselves.
 *
 * The cost is the span of a simulation, because a 64-bit count of femtoseconds runs out after
 * about 2.5 hours of simulated time. That is far longer than any ranging scenario needs.
 *
 * The resolution of ns-3 may only be set once, and only before any Time that matters has been
 * created, so call this at the top of main(). Calling it again is harmless, and UwbHelper calls
 * it as well, so a script that uses the helper before creating any Time need not call it at all.
 *
 * @return true if the resolution is now femtoseconds
 */
bool EnableUwbTimeResolution();

/**
 * @ingroup uwb
 * Convert a propagation delay into the distance it corresponds to.
 *
 * @param timeOfFlight the propagation delay
 * @return the distance, in metres
 */
double TimeOfFlightToDistance(Time timeOfFlight);

/**
 * @ingroup uwb
 * Convert a distance into the propagation delay it causes.
 *
 * @param distance the distance, in metres
 * @return the propagation delay
 */
Time DistanceToTimeOfFlight(double distance);

/**
 * @ingroup uwb
 * Round a time to the resolution of the timestamps a receiver produces.
 *
 * @param value the time
 * @return the time rounded to a whole number of device time units
 */
Time QuantiseToTimestamp(Time value);

/**
 * @ingroup uwb
 * Compute the frame check sequence of IEEE Std 802.15.4, a 16-bit cyclic redundancy check with
 * the polynomial x^16 + x^12 + x^5 + 1, processed least significant bit first.
 *
 * @param data the octets to protect
 * @param length the number of octets
 * @return the frame check sequence
 */
uint16_t Fcs16(const uint8_t* data, std::size_t length);

/**
 * @ingroup uwb
 * @param data the octets to protect
 * @return the frame check sequence
 */
uint16_t Fcs16(const std::vector<uint8_t>& data);

/**
 * @ingroup uwb
 * Estimate a position from ranges to known anchors by least squares trilateration.
 *
 * The first anchor is used as the reference to linearise the system of circle equations, which
 * is then solved in the least squares sense. At least three anchors are needed in a plane and
 * four to resolve height.
 *
 * @param anchors the positions of the anchors
 * @param ranges the measured distance to each anchor, in metres
 * @param solveHeight whether the vertical coordinate is estimated as well
 * @param position filled with the estimated position
 * @return true if a position could be computed
 */
bool Trilaterate(const std::vector<Vector>& anchors,
                 const std::vector<double>& ranges,
                 bool solveHeight,
                 Vector& position);

/**
 * @ingroup uwb
 * Solve an overdetermined linear system in the least squares sense, through its normal
 * equations and Gauss-Jordan elimination with partial pivoting.
 *
 * Both position solvers of this module end in a system of this shape, one from circles and one
 * from hyperbolas, so the arithmetic lives here once.
 *
 * @param a the matrix, with one row per equation
 * @param b the right hand side, one entry per row
 * @param x filled with the solution
 * @return true if the system had a solution, false if it was singular or underdetermined
 */
bool SolveLeastSquares(const std::vector<std::vector<double>>& a,
                       const std::vector<double>& b,
                       std::vector<double>& x);

} // namespace uwb
} // namespace ns3

#endif /* UWB_UTILS_H */
