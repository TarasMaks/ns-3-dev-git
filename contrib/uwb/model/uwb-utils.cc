/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "uwb-utils.h"

#include "ns3/abort.h"

#include <array>
#include <cmath>
#include <sstream>

namespace ns3
{
namespace uwb
{

/* -------------------------------------------------------------------------------------------- */
/* Constants                                                                                      */
/* -------------------------------------------------------------------------------------------- */

const char*
UwbDataRateName(UwbDataRate rate)
{
    switch (rate)
    {
    case UwbDataRate::RATE_110K:
        return "110kbps";
    case UwbDataRate::RATE_850K:
        return "850kbps";
    case UwbDataRate::RATE_6M81:
        return "6.81Mbps";
    case UwbDataRate::RATE_27M24:
        return "27.24Mbps";
    default:
        return "invalid";
    }
}

UwbDataRate
UwbDataRateFromString(const std::string& name)
{
    if (name == "110k" || name == "110kbps" || name == "0.11M")
    {
        return UwbDataRate::RATE_110K;
    }
    if (name == "850k" || name == "850kbps" || name == "0.85M")
    {
        return UwbDataRate::RATE_850K;
    }
    if (name == "6M81" || name == "6.81M" || name == "6.81Mbps" || name == "6M8")
    {
        return UwbDataRate::RATE_6M81;
    }
    if (name == "27M24" || name == "27.24M" || name == "27.24Mbps")
    {
        return UwbDataRate::RATE_27M24;
    }
    NS_ABORT_MSG("Unknown data rate " << name << " (use 110k, 850k, 6M81 or 27M24)");
    return UwbDataRate::RATE_6M81;
}

const char*
UwbRangingMethodName(UwbRangingMethod method)
{
    switch (method)
    {
    case UwbRangingMethod::SS_TWR:
        return "SS-TWR";
    case UwbRangingMethod::DS_TWR:
        return "DS-TWR";
    case UwbRangingMethod::TDOA:
        return "TDOA";
    default:
        return "invalid";
    }
}

UwbRangingMethod
UwbRangingMethodFromString(const std::string& name)
{
    if (name == "SS-TWR" || name == "SS" || name == "ss-twr")
    {
        return UwbRangingMethod::SS_TWR;
    }
    if (name == "DS-TWR" || name == "DS" || name == "ds-twr")
    {
        return UwbRangingMethod::DS_TWR;
    }
    if (name == "TDOA" || name == "tdoa" || name == "TDoA")
    {
        return UwbRangingMethod::TDOA;
    }
    NS_ABORT_MSG("Unknown ranging method " << name << " (use SS-TWR, DS-TWR or TDOA)");
    return UwbRangingMethod::DS_TWR;
}

const char*
UwbPrfName(UwbPrf prf)
{
    return (prf == UwbPrf::PRF_16M) ? "PRF16" : "PRF64";
}

namespace
{

/// Centre frequency of every channel this model supports, in MHz, indexed by channel number.
constexpr std::array<double, 16> CHANNEL_FREQUENCY_MHZ{
    0.0,      // channel 0 is in the sub-gigahertz band and is not modelled
    3494.4,   // 1
    3993.6,   // 2
    4492.8,   // 3, mandatory in the low band
    3993.6,   // 4, a wide channel centred like channel 2
    6489.6,   // 5, used by most products
    6988.8,   // 6
    6489.6,   // 7, a wide channel centred like channel 5
    7488.0,   // 8
    7987.2,   // 9, mandatory in the high band and used by most products
    8486.4,   // 10
    7987.2,   // 11, a wide channel centred like channel 9
    8985.6,   // 12
    9484.8,   // 13
    9984.0,   // 14
    9484.8};  // 15, a wide channel centred like channel 13

/// Bandwidth of every channel, in MHz, indexed by channel number.
constexpr std::array<double, 16> CHANNEL_BANDWIDTH_MHZ{
    0.0,     499.2,  499.2,  499.2,  1331.2, 499.2, 499.2, 1081.6,
    499.2,   499.2,  499.2,  1331.2, 499.2,  499.2, 499.2, 1354.97};

} // namespace

double
ChannelToFrequencyMhz(uint8_t channel)
{
    NS_ABORT_MSG_IF(channel < FIRST_CHANNEL || channel > LAST_CHANNEL,
                    "Channel " << +channel << " is outside the range this model supports");
    return CHANNEL_FREQUENCY_MHZ[channel];
}

double
ChannelToBandwidthMhz(uint8_t channel)
{
    NS_ABORT_MSG_IF(channel < FIRST_CHANNEL || channel > LAST_CHANNEL,
                    "Channel " << +channel << " is outside the range this model supports");
    return CHANNEL_BANDWIDTH_MHZ[channel];
}

bool
IsMandatoryChannel(uint8_t channel)
{
    // channel 0 is mandatory in the sub-gigahertz band, which this model does not cover
    return channel == 3 || channel == 9;
}

double
GetSymbolDurationS(UwbDataRate rate)
{
    // the symbol durations of Clause 15, expressed in seconds
    switch (rate)
    {
    case UwbDataRate::RATE_110K:
        return 8205.13e-9;
    case UwbDataRate::RATE_850K:
        return 1025.64e-9;
    case UwbDataRate::RATE_6M81:
        return 128.21e-9;
    case UwbDataRate::RATE_27M24:
        return 32.05e-9;
    default:
        NS_ABORT_MSG("Invalid data rate");
    }
    return 0.0;
}

double
GetBitRate(UwbDataRate rate)
{
    // one symbol carries one coded bit, and the Reed-Solomon code passes 55 information bits
    // for every 63 coded ones
    return (1.0 / GetSymbolDurationS(rate)) * 55.0 / 63.0;
}

double
GetPreambleSymbolDurationS(UwbPrf prf)
{
    return (prf == UwbPrf::PRF_16M) ? PREAMBLE_SYMBOL_16M_S : PREAMBLE_SYMBOL_64M_S;
}

UwbDataRate
GetPhrDataRate(UwbDataRate rate)
{
    // the header travels at 0.11 Mb/s in the long range mode and at 0.85 Mb/s otherwise
    return (rate == UwbDataRate::RATE_110K) ? UwbDataRate::RATE_110K : UwbDataRate::RATE_850K;
}

/* -------------------------------------------------------------------------------------------- */
/* Frame timing                                                                                   */
/* -------------------------------------------------------------------------------------------- */

uint32_t
UwbPhyConfig::GetSfdSymbols() const
{
    // the long delimiter is used with the long range mode
    return (dataRate == UwbDataRate::RATE_110K) ? SFD_SYMBOLS_LONG : SFD_SYMBOLS_SHORT;
}

std::string
UwbPhyConfig::ToString() const
{
    std::ostringstream oss;
    oss << "channel " << +channel << " (" << ChannelToFrequencyMhz(channel) << " MHz, "
        << ChannelToBandwidthMhz(channel) << " MHz wide), " << UwbDataRateName(dataRate) << ", "
        << UwbPrfName(prf) << ", preamble " << preambleSymbols;
    return oss.str();
}

uint32_t
GetReedSolomonCodedBits(uint32_t octets)
{
    if (octets == 0)
    {
        return 0;
    }
    const uint32_t informationBits = octets * 8;
    const uint32_t blocks =
        (informationBits + RS_BLOCK_INFORMATION_BITS - 1) / RS_BLOCK_INFORMATION_BITS;
    return informationBits + blocks * RS_BLOCK_PARITY_BITS;
}

Time
GetShrDuration(const UwbPhyConfig& config)
{
    const double symbols = config.preambleSymbols + config.GetSfdSymbols();
    return Seconds(symbols * GetPreambleSymbolDurationS(config.prf));
}

Time
GetPhrDuration(const UwbPhyConfig& config)
{
    return Seconds(PHR_BITS * GetSymbolDurationS(GetPhrDataRate(config.dataRate)));
}

Time
GetPsduDuration(const UwbPhyConfig& config, uint32_t psduOctets)
{
    return Seconds(GetReedSolomonCodedBits(psduOctets) * GetSymbolDurationS(config.dataRate));
}

Time
GetFrameDuration(const UwbPhyConfig& config, uint32_t psduOctets)
{
    return GetShrDuration(config) + GetPhrDuration(config) + GetPsduDuration(config, psduOctets);
}

double
GetProcessingGainDb(const UwbPhyConfig& config)
{
    const double bandwidthHz = ChannelToBandwidthMhz(config.channel) * 1e6;
    return 10.0 * std::log10(bandwidthHz / GetBitRate(config.dataRate));
}

double
GetRegulatoryTxPowerDbm(uint8_t channel, double densityDbmPerMhz)
{
    return densityDbmPerMhz + 10.0 * std::log10(ChannelToBandwidthMhz(channel));
}

bool
EnableUwbTimeResolution()
{
    if (Time::GetResolution() != Time::FS)
    {
        Time::SetResolution(Time::FS);
    }
    return Time::GetResolution() == Time::FS;
}

double
TimeOfFlightToDistance(Time timeOfFlight)
{
    return timeOfFlight.GetSeconds() * SPEED_OF_LIGHT;
}

Time
DistanceToTimeOfFlight(double distance)
{
    return Seconds(distance / SPEED_OF_LIGHT);
}

Time
QuantiseToTimestamp(Time value)
{
    const double units = std::round(value.GetSeconds() / TIMESTAMP_RESOLUTION_S);
    return Seconds(units * TIMESTAMP_RESOLUTION_S);
}

/* -------------------------------------------------------------------------------------------- */
/* Frame check sequence                                                                           */
/* -------------------------------------------------------------------------------------------- */

uint16_t
Fcs16(const uint8_t* data, std::size_t length)
{
    // the polynomial x^16 + x^12 + x^5 + 1 in its reflected form
    constexpr uint16_t POLYNOMIAL = 0x8408;
    uint16_t fcs = 0;
    for (std::size_t i = 0; i < length; ++i)
    {
        fcs ^= data[i];
        for (uint8_t bit = 0; bit < 8; ++bit)
        {
            if (fcs & 0x0001)
            {
                fcs = static_cast<uint16_t>((fcs >> 1) ^ POLYNOMIAL);
            }
            else
            {
                fcs = static_cast<uint16_t>(fcs >> 1);
            }
        }
    }
    return fcs;
}

uint16_t
Fcs16(const std::vector<uint8_t>& data)
{
    return Fcs16(data.data(), data.size());
}

/* -------------------------------------------------------------------------------------------- */
/* Trilateration                                                                                  */
/* -------------------------------------------------------------------------------------------- */

bool
Trilaterate(const std::vector<Vector>& anchors,
            const std::vector<double>& ranges,
            bool solveHeight,
            Vector& position)
{
    const std::size_t unknowns = solveHeight ? 3 : 2;
    if (anchors.size() != ranges.size() || anchors.size() < unknowns + 1)
    {
        return false;
    }

    // Subtracting the equation of the first anchor from each of the others removes the quadratic
    // terms and leaves a linear system, which is then solved in the least squares sense through
    // its normal equations.
    const Vector& reference = anchors[0];
    const double referenceRange = ranges[0];
    const std::size_t rows = anchors.size() - 1;

    std::vector<std::vector<double>> a(rows, std::vector<double>(unknowns, 0.0));
    std::vector<double> b(rows, 0.0);

    for (std::size_t i = 0; i < rows; ++i)
    {
        const Vector& anchor = anchors[i + 1];
        a[i][0] = 2.0 * (anchor.x - reference.x);
        a[i][1] = 2.0 * (anchor.y - reference.y);
        if (solveHeight)
        {
            a[i][2] = 2.0 * (anchor.z - reference.z);
        }
        const double anchorNorm = anchor.x * anchor.x + anchor.y * anchor.y + anchor.z * anchor.z;
        const double referenceNorm = reference.x * reference.x + reference.y * reference.y +
                                     reference.z * reference.z;
        b[i] = referenceRange * referenceRange - ranges[i + 1] * ranges[i + 1] + anchorNorm -
               referenceNorm;
        if (!solveHeight)
        {
            // the vertical coordinate is taken from the anchors and moved to the right hand side
            b[i] -= 2.0 * (anchor.z - reference.z) * reference.z;
        }
    }

    // normal equations: (A^T A) x = A^T b
    std::vector<std::vector<double>> normal(unknowns, std::vector<double>(unknowns + 1, 0.0));
    for (std::size_t r = 0; r < unknowns; ++r)
    {
        for (std::size_t c = 0; c < unknowns; ++c)
        {
            double sum = 0.0;
            for (std::size_t i = 0; i < rows; ++i)
            {
                sum += a[i][r] * a[i][c];
            }
            normal[r][c] = sum;
        }
        double sum = 0.0;
        for (std::size_t i = 0; i < rows; ++i)
        {
            sum += a[i][r] * b[i];
        }
        normal[r][unknowns] = sum;
    }

    // Gaussian elimination with partial pivoting
    for (std::size_t column = 0; column < unknowns; ++column)
    {
        std::size_t pivot = column;
        for (std::size_t row = column + 1; row < unknowns; ++row)
        {
            if (std::abs(normal[row][column]) > std::abs(normal[pivot][column]))
            {
                pivot = row;
            }
        }
        if (std::abs(normal[pivot][column]) < 1e-12)
        {
            // the anchors are collinear, or coplanar when height is being solved for
            return false;
        }
        std::swap(normal[column], normal[pivot]);
        for (std::size_t row = 0; row < unknowns; ++row)
        {
            if (row == column)
            {
                continue;
            }
            const double factor = normal[row][column] / normal[column][column];
            for (std::size_t c = column; c <= unknowns; ++c)
            {
                normal[row][c] -= factor * normal[column][c];
            }
        }
    }

    position.x = normal[0][unknowns] / normal[0][0];
    position.y = normal[1][unknowns] / normal[1][1];
    position.z = solveHeight ? normal[2][unknowns] / normal[2][2] : reference.z;
    return true;
}

} // namespace uwb
} // namespace ns3
