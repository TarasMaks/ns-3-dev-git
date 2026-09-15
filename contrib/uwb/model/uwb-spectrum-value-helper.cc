/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "uwb-spectrum-value-helper.h"

#include "uwb-utils.h"

#include "ns3/abort.h"
#include "ns3/log.h"

#include <algorithm>
#include <cmath>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("UwbSpectrumValueHelper");

namespace uwb
{

namespace
{

/**
 * Width of one bin of the spectrum model, in Hz. A fifth of the 499.2 MHz channel bandwidth,
 * which makes every channel exactly five bins wide.
 */
constexpr double BIN_WIDTH_HZ = 499.2e6 / 5.0;
/// Centre of the lowest bin, in Hz, just below the 3.1 GHz edge of the band.
constexpr double FIRST_BIN_CENTRE_HZ = 31 * BIN_WIDTH_HZ;
/// Number of bins, which together reach past the 10.6 GHz edge of the band.
constexpr uint32_t N_BINS = 77;
/// Boltzmann constant multiplied by the reference temperature of 290 K, in W/Hz.
constexpr double KT_W_PER_HZ = 1.38064852e-23 * 290.0;

/**
 * Attenuation of the out of band emission relative to the in band density, in dB. The pulse
 * shape of Clause 15 is a root raised cosine, whose first sidelobes the transmit mask of the
 * standard holds at least this far down.
 */
constexpr double OUT_OF_BAND_ATTENUATION_DB = 30.0;
/// Width of the skirt modelled on each side of the channel, as a fraction of its bandwidth.
constexpr double SKIRT_FRACTION = 0.5;

/// Global spectrum model, built once and shared by every UWB PHY.
Ptr<SpectrumModel> g_uwbSpectrumModel;

/// @param index a bin index
/// @return the lowest frequency of the bin, in Hz
double
BinLow(uint32_t index)
{
    return FIRST_BIN_CENTRE_HZ + index * BIN_WIDTH_HZ - BIN_WIDTH_HZ / 2;
}

/// @param index a bin index
/// @return the highest frequency of the bin, in Hz
double
BinHigh(uint32_t index)
{
    return BinLow(index) + BIN_WIDTH_HZ;
}

/**
 * The fraction of a bin that a frequency interval covers.
 *
 * @param index the bin index
 * @param low the lowest frequency of the interval, in Hz
 * @param high the highest frequency of the interval, in Hz
 * @return a fraction between 0 and 1
 */
double
Overlap(uint32_t index, double low, double high)
{
    const double from = std::max(low, BinLow(index));
    const double to = std::min(high, BinHigh(index));
    return (to <= from) ? 0.0 : (to - from) / BIN_WIDTH_HZ;
}

/**
 * The relative power density of a channel in every bin: one across the channel, a skirt on each
 * side, zero elsewhere. Bins that straddle an edge take their share.
 *
 * @param channel the channel index
 * @return one weight per bin
 */
std::vector<double>
GetChannelWeights(uint8_t channel)
{
    const double centreHz = ChannelToFrequencyMhz(channel) * 1e6;
    const double bandwidthHz = ChannelToBandwidthMhz(channel) * 1e6;
    const double low = centreHz - bandwidthHz / 2;
    const double high = centreHz + bandwidthHz / 2;
    const double skirt = bandwidthHz * SKIRT_FRACTION;
    const double skirtWeight = std::pow(10.0, -OUT_OF_BAND_ATTENUATION_DB / 10.0);

    std::vector<double> weights(N_BINS, 0.0);
    for (uint32_t i = 0; i < N_BINS; ++i)
    {
        weights[i] = Overlap(i, low, high) +
                     skirtWeight * (Overlap(i, low - skirt, low) + Overlap(i, high, high + skirt));
    }
    return weights;
}

} // namespace

Ptr<const SpectrumModel>
UwbSpectrumValueHelper::GetSpectrumModel()
{
    if (!g_uwbSpectrumModel)
    {
        Bands bands;
        for (uint32_t i = 0; i < N_BINS; ++i)
        {
            BandInfo band;
            band.fl = BinLow(i);
            band.fc = BinLow(i) + BIN_WIDTH_HZ / 2;
            band.fh = BinHigh(i);
            bands.push_back(band);
        }
        g_uwbSpectrumModel = Create<SpectrumModel>(bands);
    }
    return g_uwbSpectrumModel;
}

Ptr<SpectrumValue>
UwbSpectrumValueHelper::CreateTxPowerSpectralDensity(double txPowerDbm, uint8_t channel)
{
    auto psd = Create<SpectrumValue>(GetSpectrumModel());
    const double txPowerW = std::pow(10.0, (txPowerDbm - 30.0) / 10.0);
    const auto weights = GetChannelWeights(channel);

    // scale the weights so that the density integrates to exactly the transmitted power, which
    // keeps the skirts from quietly adding energy to the signal
    double total = 0.0;
    for (const double weight : weights)
    {
        total += weight * BIN_WIDTH_HZ;
    }
    NS_ABORT_MSG_IF(total <= 0.0, "Channel " << +channel << " falls outside the spectrum model");

    for (uint32_t i = 0; i < N_BINS; ++i)
    {
        (*psd)[i] = txPowerW * weights[i] / total;
    }
    return psd;
}

Ptr<SpectrumValue>
UwbSpectrumValueHelper::CreateNoisePowerSpectralDensity(double noiseFigureDb)
{
    auto psd = Create<SpectrumValue>(GetSpectrumModel());
    const double noisePsd = KT_W_PER_HZ * std::pow(10.0, noiseFigureDb / 10.0);
    for (uint32_t i = 0; i < N_BINS; ++i)
    {
        (*psd)[i] = noisePsd;
    }
    return psd;
}

Ptr<SpectrumValue>
UwbSpectrumValueHelper::CreateRfFilter(uint8_t channel)
{
    auto filter = Create<SpectrumValue>(GetSpectrumModel());
    const double centreHz = ChannelToFrequencyMhz(channel) * 1e6;
    const double bandwidthHz = ChannelToBandwidthMhz(channel) * 1e6;
    for (uint32_t i = 0; i < N_BINS; ++i)
    {
        // a bin that the channel reaches into at all is passed whole. The alternative, weighing
        // such a bin by the fraction of it that is in band, would multiply a density that has
        // already been averaged over the whole bin by that fraction a second time, and so lose
        // part of the wanted signal. Nothing is lost here: the eleven channels of nominal width,
        // which include both mandatory ones and every channel products use, are a whole number
        // of bins wide and land on bin edges, so the filter is exact for them. The four wide
        // channels are not, and for those the filter reaches at most half a bin past each edge.
        (*filter)[i] =
            (Overlap(i, centreHz - bandwidthHz / 2, centreHz + bandwidthHz / 2) > 1e-9) ? 1.0 : 0.0;
    }
    return filter;
}

double
UwbSpectrumValueHelper::GetBandPower(Ptr<const SpectrumValue> psd, uint8_t channel)
{
    if (!psd)
    {
        return 0.0;
    }
    auto filter = CreateRfFilter(channel);
    double power = 0.0;
    auto psdIt = psd->ConstValuesBegin();
    auto filterIt = filter->ConstValuesBegin();
    auto bandIt = psd->ConstBandsBegin();
    while (psdIt != psd->ConstValuesEnd() && filterIt != filter->ConstValuesEnd())
    {
        power += (*psdIt) * (*filterIt) * (bandIt->fh - bandIt->fl);
        ++psdIt;
        ++filterIt;
        ++bandIt;
    }
    return power;
}

double
UwbSpectrumValueHelper::GetNoiseBandwidthHz(uint8_t channel)
{
    return ChannelToBandwidthMhz(channel) * 1e6;
}

double
UwbSpectrumValueHelper::GetPowerDensityDbmPerMhz(double txPowerDbm, uint8_t channel)
{
    return txPowerDbm - 10.0 * std::log10(ChannelToBandwidthMhz(channel));
}

double
UwbSpectrumValueHelper::GetBinWidthHz()
{
    return BIN_WIDTH_HZ;
}

uint32_t
UwbSpectrumValueHelper::GetNBins()
{
    return N_BINS;
}

} // namespace uwb
} // namespace ns3
