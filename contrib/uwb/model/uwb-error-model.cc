/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "uwb-error-model.h"

#include "uwb-spectrum-value-helper.h"

#include "ns3/abort.h"
#include "ns3/assert.h"
#include "ns3/double.h"
#include "ns3/log.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("UwbErrorModel");

namespace uwb
{

namespace
{

/// Boltzmann constant multiplied by the reference temperature of 290 K, in W/Hz.
constexpr double KT_W_PER_HZ = 1.38064852e-23 * 290.0;

/// @param prf a mean pulse repetition frequency
/// @return the length of a preamble code symbol, in chips
double
GetPreambleCodeLength(UwbPrf prf)
{
    return (prf == UwbPrf::PRF_16M) ? 31.0 : 127.0;
}

/**
 * Solve the per rate term of the error model from the two published sensitivities.
 *
 * The term is the net of the coding gain of Clause 15 and the implementation loss of a real
 * receiver. At each anchor the term is whatever makes the modelled sensitivity land on the
 * published figure; the other two rates sit on the straight line the anchors define against
 * the logarithm of the data rate.
 *
 * @return the net gain of each data rate, in dB
 */
std::array<double, static_cast<std::size_t>(UwbDataRate::DATA_RATE_COUNT)>
SolveNetCodingGains()
{
    constexpr auto COUNT = static_cast<std::size_t>(UwbDataRate::DATA_RATE_COUNT);
    const uint64_t nbits = static_cast<uint64_t>(SENSITIVITY_PAYLOAD_OCTETS) * 8;
    const double required = UwbErrorModel::GetRequiredEffectiveEbNoDb(nbits, SENSITIVITY_PER);

    // the gain each anchor demands, and where it sits on the logarithmic rate axis
    const auto anchorGain = [required](UwbDataRate rate, double sensitivityDbm) {
        const double rb = GetBitRate(rate);
        const double noiseDbm =
            UwbErrorModel::GetThermalNoiseDbm(rb, UwbErrorModel::REFERENCE_NOISE_FIGURE_DB);
        return required - (sensitivityDbm - noiseDbm);
    };

    const double lowX = std::log10(GetBitRate(UwbDataRate::RATE_110K));
    const double highX = std::log10(GetBitRate(UwbDataRate::RATE_6M81));
    const double lowY = anchorGain(UwbDataRate::RATE_110K, PUBLISHED_SENSITIVITY_110K_DBM);
    const double highY = anchorGain(UwbDataRate::RATE_6M81, PUBLISHED_SENSITIVITY_6M81_DBM);
    const double slope = (highY - lowY) / (highX - lowX);

    std::array<double, COUNT> gains{};
    for (std::size_t i = 0; i < COUNT; ++i)
    {
        const double x = std::log10(GetBitRate(static_cast<UwbDataRate>(i)));
        gains[i] = lowY + slope * (x - lowX);
    }
    return gains;
}

} // namespace

NS_OBJECT_ENSURE_REGISTERED(UwbErrorModel);

TypeId
UwbErrorModel::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::uwb::UwbErrorModel")
            .SetParent<Object>()
            .SetGroupName("Uwb")
            .AddConstructor<UwbErrorModel>()
            .AddAttribute("NoiseFigure",
                          "Noise figure of the receiver, in dB. The default is the figure the "
                          "published sensitivities this model is calibrated against were "
                          "measured at; raising it moves every sensitivity down by the same "
                          "amount.",
                          DoubleValue(REFERENCE_NOISE_FIGURE_DB),
                          MakeDoubleAccessor(&UwbErrorModel::m_noiseFigureDb),
                          MakeDoubleChecker<double>(0.0, 30.0))
            .AddAttribute("AcquisitionThreshold",
                          "Correlator output, in dB, that the preamble has to reach before the "
                          "receiver locks onto a frame. The default leaves several dB of margin "
                          "at the preamble lengths products use, so the payload sets the "
                          "sensitivity, while short preambles at long range still fail to "
                          "acquire.",
                          DoubleValue(15.0),
                          MakeDoubleAccessor(&UwbErrorModel::m_acquisitionThresholdDb),
                          MakeDoubleChecker<double>(0.0, 40.0))
            .AddAttribute("CodeRejection",
                          "Rejection, in dB, applied to a UWB signal that uses a different "
                          "preamble code from the one being received. The ternary codes of "
                          "Clause 15 have low but not zero cross correlation, so a foreign code "
                          "interferes far less than a matching one, which is what lets many UWB "
                          "links share a channel.",
                          DoubleValue(18.0),
                          MakeDoubleAccessor(&UwbErrorModel::m_codeRejectionDb),
                          MakeDoubleChecker<double>(0.0, 40.0))
            .AddAttribute("ExtraImplementationLoss",
                          "Loss added, in dB, on top of the loss already folded into the "
                          "calibration. Use it to model a receiver worse than the one the "
                          "published sensitivities come from.",
                          DoubleValue(0.0),
                          MakeDoubleAccessor(&UwbErrorModel::m_extraImplementationLossDb),
                          MakeDoubleChecker<double>(0.0, 30.0));
    return tid;
}

UwbErrorModel::UwbErrorModel()
    : m_noiseFigureDb(REFERENCE_NOISE_FIGURE_DB),
      m_acquisitionThresholdDb(15.0),
      m_codeRejectionDb(18.0),
      m_extraImplementationLossDb(0.0)
{
    NS_LOG_FUNCTION(this);
}

UwbErrorModel::~UwbErrorModel()
{
    NS_LOG_FUNCTION(this);
}

double
UwbErrorModel::GetThermalNoiseDbm(double bandwidthHz, double noiseFigureDb)
{
    return 10.0 * std::log10(KT_W_PER_HZ * bandwidthHz * 1000.0) + noiseFigureDb;
}

double
UwbErrorModel::GetRequiredEffectiveEbNoDb(uint64_t nbits, double per)
{
    NS_ASSERT_MSG(nbits > 0, "A frame with no bits has no error rate");
    NS_ASSERT_MSG(per > 0.0 && per < 1.0, "A packet error rate has to lie strictly inside 0 to 1");

    // the bit error rate that leaves the whole frame correct with probability 1 - per
    const double ber = -std::expm1(std::log1p(-per) / static_cast<double>(nbits));
    if (ber >= 0.5)
    {
        return -std::numeric_limits<double>::infinity();
    }

    // invert BER = 0.5 erfc(sqrt(Eb/N0)); erfc is monotone, so bisect on it
    double low = 1e-6;
    double high = 1e3;
    for (int i = 0; i < 200; ++i)
    {
        const double mid = 0.5 * (low + high);
        if (0.5 * std::erfc(std::sqrt(mid)) > ber)
        {
            low = mid;
        }
        else
        {
            high = mid;
        }
    }
    return 10.0 * std::log10(0.5 * (low + high));
}

double
UwbErrorModel::GetNetCodingGainDb(UwbDataRate rate)
{
    static const auto gains = SolveNetCodingGains();
    const auto index = static_cast<std::size_t>(rate);
    NS_ABORT_MSG_IF(index >= gains.size(), "Invalid data rate");
    return gains[index];
}

double
UwbErrorModel::GetEbNoDb(double sinr, const UwbPhyConfig& config) const
{
    if (sinr <= 0.0)
    {
        return -std::numeric_limits<double>::infinity();
    }
    return 10.0 * std::log10(sinr) + GetProcessingGainDb(config);
}

double
UwbErrorModel::GetBitErrorRate(double sinr, const UwbPhyConfig& config) const
{
    const double ebNoDb = GetEbNoDb(sinr, config) + GetNetCodingGainDb(config.dataRate) -
                          m_extraImplementationLossDb;
    if (!std::isfinite(ebNoDb))
    {
        return 0.5;
    }
    const double ebNo = std::pow(10.0, ebNoDb / 10.0);
    return std::min(0.5, 0.5 * std::erfc(std::sqrt(ebNo)));
}

double
UwbErrorModel::GetChunkSuccessRate(double sinr, const UwbPhyConfig& config, uint64_t nbits) const
{
    if (nbits == 0)
    {
        return 1.0;
    }
    const double ber = GetBitErrorRate(sinr, config);
    if (ber <= 0.0)
    {
        return 1.0;
    }
    return std::exp(static_cast<double>(nbits) * std::log1p(-ber));
}

double
UwbErrorModel::GetAcquisitionGainDb(const UwbPhyConfig& config) const
{
    const double accumulated = GetPreambleCodeLength(config.prf) * config.preambleSymbols;
    return 10.0 * std::log10(accumulated);
}

bool
UwbErrorModel::IsPreambleAcquired(double sinr, const UwbPhyConfig& config) const
{
    if (sinr <= 0.0)
    {
        return false;
    }
    const double correlated = 10.0 * std::log10(sinr) + GetAcquisitionGainDb(config) -
                              m_extraImplementationLossDb;
    return correlated >= m_acquisitionThresholdDb;
}

double
UwbErrorModel::GetSensitivityDbm(const UwbPhyConfig& config, uint32_t psduOctets, double per) const
{
    const uint64_t nbits = static_cast<uint64_t>(psduOctets) * 8;
    const double required = GetRequiredEffectiveEbNoDb(nbits, per) -
                            GetNetCodingGainDb(config.dataRate) + m_extraImplementationLossDb;
    return GetThermalNoiseDbm(GetBitRate(config.dataRate), m_noiseFigureDb) + required;
}

double
UwbErrorModel::GetAcquisitionThresholdDbm(const UwbPhyConfig& config) const
{
    const double bandwidthHz = UwbSpectrumValueHelper::GetNoiseBandwidthHz(config.channel);
    const double requiredSinrDb =
        m_acquisitionThresholdDb - GetAcquisitionGainDb(config) + m_extraImplementationLossDb;
    return GetThermalNoiseDbm(bandwidthHz, m_noiseFigureDb) + requiredSinrDb;
}

double
UwbErrorModel::GetNoiseFigureDb() const
{
    return m_noiseFigureDb;
}

double
UwbErrorModel::GetCodeRejectionDb() const
{
    return m_codeRejectionDb;
}

} // namespace uwb
} // namespace ns3
